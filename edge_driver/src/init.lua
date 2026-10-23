local log = require "log"
local Driver = require "st.driver"
local capabilities = require "st.capabilities"
local mdns = require "st.mdns"
local json = require "st.json"
local ltn12 = require "ltn12"
local cosock = require "cosock"
local http = cosock.asyncify "socket.http"

local PROFILE = "apc-surge-arrest.v2"
local CHILD_PROFILE = "apc-surge-switch.v1"
local LED_PROFILE = "apc-surge-led.v1"
local endpoints = {}
local pending_creates = {}
local command_generations = {}
local HTTP_TIMEOUT_SECONDS = 60
local switches = {
  main = "outlet1",
  outlet2 = "outlet2",
  outlet3 = "outlet3",
  usb_charger1 = "usb_charger1",
  usb_charger2 = "usb_charger2",
  led = "led"
}
local child_order = { "outlet1", "outlet2", "outlet3", "usb_charger1", "usb_charger2", "led" }
local child_labels = {
  outlet1 = "APC Outlet 1",
  outlet2 = "APC Outlet 2",
  outlet3 = "APC Outlet 3",
  usb_charger1 = "APC USB Charger 1",
  usb_charger2 = "APC USB Charger 2",
  led = "APC Strip LED"
}

local function request(ip, port, path, method, token)
  local response = {}
  local headers = { ["Connection"] = "close" }
  if token and #token >= 24 then headers["X-APC-Token"] = token end
  if method == "POST" then headers["Content-Length"] = "0" end
  local ok, code = http.request({
    url = string.format("http://%s:%d%s", ip, port, path),
    method = method,
    headers = headers,
    sink = ltn12.sink.table(response),
    create = function()
      local socket = cosock.socket.tcp()
      local settimeout = socket.settimeout
      socket.settimeout = function(self, _, mode)
        return settimeout(self, HTTP_TIMEOUT_SECONDS, mode)
      end
      socket:settimeout(HTTP_TIMEOUT_SECONDS)
      -- Submit the
      -- request line and authentication headers in one write to ESP WebServer.
      local send = socket.send
      local request_line
      socket.send = function(self, data, first, last)
        if request_line then
          local prefix = request_line
          request_line = nil
          local start = first or 1
          local payload = data:sub(start, last or -1)
          local sent, err, partial = send(self, prefix .. payload)
          if sent then return start + sent - #prefix - 1 end
          return nil, err, start - 1 + math.max(0, (partial or 0) - #prefix)
        end
        if not first and not last and data:match("^%u+ [^\r\n]+ HTTP/1%.1\r\n$") then
          request_line = data
          return #data
        end
        return send(self, data, first, last)
      end
      return socket
    end
  })
  if not ok then return nil, code end
  local status = tonumber(code)
  if not status or status < 200 or status >= 300 then return nil, code end
  local body = table.concat(response)
  if body == "" then return {}, status end
  local decoded_ok, decoded = pcall(json.decode, body)
  if not decoded_ok then return nil, "invalid JSON" end
  return decoded, status
end

local function dni(bridge_id)
  return "apc-surge-" .. bridge_id:gsub("[^%w]", ""):upper()
end

local function existing_device(driver, network_id)
  for _, device in ipairs(driver:get_devices()) do
    if device.device_network_id == network_id then return device end
  end
end

local function is_parent(device)
  return device:component_exists("outlet2") and
    device:component_exists("usb_charger2")
end

local function create_children(driver, parent)
  for _, property in ipairs(child_order) do
    local key = parent.id .. ":" .. property
    if parent:get_child_by_parent_assigned_key(property) then
      pending_creates[key] = nil
    elseif not pending_creates[key] then
      local label = child_labels[property]
      driver:try_create_device({
        type = "EDGE_CHILD", label = label,
        profile = property == "led" and LED_PROFILE or CHILD_PROFILE,
        parent_device_id = parent.id, parent_assigned_child_key = property,
        vendor_provided_label = label,
        manufacturer = "APC", model = "PH6U4X32W"
      })
      pending_creates[key] = true
    end
  end
end

-- Resolve child keys through the parent so no child metadata format is assumed.
local function child_context(device)
  local parent = device:get_parent_device()
  if not parent then return nil end
  for _, property in ipairs(child_order) do
    local child = parent:get_child_by_parent_assigned_key(property)
    if child and child.id == device.id then return parent, property end
  end
  return nil
end

local function is_ipv4(address)
  return type(address) == "string" and
    address:match("^%d+%.%d+%.%d+%.%d+$") ~= nil
end

local function find_bridges(driver, create_devices)
  local result = mdns.discover("_surgearrest._tcp", "local")
  for _, found in ipairs((result and result.found) or {}) do
    local host = found.host_info
    if host and is_ipv4(host.address) and type(host.port) == "number" then
      local info = request(host.address, host.port, "/v1/info", "GET")
      if info and info.apiVersion == 1 and type(info.bridgeId) == "string" and
          info.apc and info.apc.configured and #(info.apc.switches or {}) >= 3 then
        local network_id = dni(info.bridgeId)
        endpoints[network_id] = { ip = host.address, port = host.port }
        local device = existing_device(driver, network_id)
        if device then
          device:set_field("bridge_ip", host.address, { persist = true })
          device:set_field("bridge_port", host.port, { persist = true })
          create_children(driver, device)
        elseif create_devices and not pending_creates[network_id] then
          driver:try_create_device({ type = "LAN", device_network_id = network_id,
            label = "APC SurgeArrest", vendor_provided_label = "APC SurgeArrest", profile = PROFILE,
            manufacturer = "APC", model = "PH6U4X32W" })
          pending_creates[network_id] = true
        end
      end
    end
  end
end

local function endpoint_for(driver, device)
  local endpoint = endpoints[device.device_network_id]
  if endpoint then return endpoint end
  local ip = device:get_field("bridge_ip")
  local port = device:get_field("bridge_port")
  if ip and port then return { ip = ip, port = port } end
  find_bridges(driver, false)
  return endpoints[device.device_network_id]
end

local function refresh(driver, device)
  local parent, property
  if is_parent(device) then
    parent = device
  else
    parent, property = child_context(device)
  end
  if not parent then return end
  local endpoint = endpoint_for(driver, parent)
  local token = parent.preferences.apcToken
  if not endpoint or type(token) ~= "string" or #token < 24 then return end
  local state, err = request(endpoint.ip, endpoint.port, "/v1/apc", "GET", token)
  if not state then
    log.warn("APC local status unavailable: " .. tostring(err))
    parent:offline()
    return
  end
  parent:online()
  for component, property in pairs(switches) do
    local value = state.switches and state.switches[property]
    if type(value) == "boolean" and parent.profile.components[component] then
      parent:emit_component_event(parent.profile.components[component],
        capabilities.switch.switch(value and "on" or "off"))
    end
  end
  for _, property in ipairs(child_order) do
    local child = parent:get_child_by_parent_assigned_key(property)
    if child then
      child:online()
      local value = state.switches and state.switches[property]
      if type(value) == "boolean" then
        child:emit_event(capabilities.switch.switch(value and "on" or "off"))
      end
    end
    local delivery = state.commands and state.commands[property]
    if delivery and delivery.status == "unconfirmed" then
      local field = "apc_unconfirmed_" .. property
      if parent:get_field(field) ~= delivery.id then
        parent:set_field(field, delivery.id)
        log.warn(string.format("APC %s request %s remains unconfirmed after %s attempts; physical state is unknown",
          property, tostring(delivery.id), tostring(delivery.attempts)))
      end
    end
  end
end

local function set_switch(driver, device, command, value)
  local parent, property
  if is_parent(device) then
    parent = device
    property = switches[command.component or "main"]
  else
    parent, property = child_context(device)
  end
  local endpoint = parent and endpoint_for(driver, parent)
  local token = parent and parent.preferences.apcToken
  if not property or not endpoint or type(token) ~= "string" or #token < 24 then
    log.warn("APC switch unavailable: missing component, bridge, or token")
    return
  end
  local path = "/v1/apc/" .. property .. "/" .. value
  local key = parent.id .. ":" .. property
  command_generations[key] = (command_generations[key] or 0) + 1
  local generation = command_generations[key]
  local function deliver(attempt)
    if command_generations[key] ~= generation then return end
    local result, err = request(endpoint.ip, endpoint.port, path, "POST", token)
    if command_generations[key] ~= generation then return end
    if not result then
      local code = tonumber(err)
      local transient = not code or code == 408 or code == 429 or code >= 500
      if transient and attempt < 3 then
        log.warn(string.format("APC %s %s delivery retry %d: %s", property, value, attempt, tostring(err)))
        driver:call_with_delay(attempt, function() deliver(attempt + 1) end)
      else
        log.warn(string.format("APC %s %s delivery failed: %s", property, value, tostring(err)))
      end
      return
    end
    -- Queued/sent is not physical state. Only the ESP's observed values emit
    -- switch events; check again after its bounded delivery/confirmation cycle.
    for _, delay in ipairs({2, 9, 18, 25}) do
      driver:call_with_delay(delay, function() refresh(driver, parent) end)
    end
  end
  deliver(1)
end

local driver = Driver("APC SurgeArrest ESP Bridge", {
  discovery = function(self, _discovery_args)
    find_bridges(self, true)
  end,
  lifecycle_handlers = {
    added = function(self, device)
      if device.device_network_id then
        pending_creates[device.device_network_id] = nil
      end
      if is_parent(device) then create_children(self, device) end
      self:call_with_delay(2, function() refresh(self, device) end)
    end,
    init = function(self, device)
      if is_parent(device) then
        if not device:component_exists("led") then
          device:try_update_metadata({ profile = PROFILE })
        end
        self:call_with_delay(5, function() create_children(self, device) end)
      end
      self:call_with_delay(2, function() refresh(self, device) end)
    end,
    infoChanged = function(self, device)
      self:call_with_delay(2, function() refresh(self, device) end)
    end
  },
  capability_handlers = {
    [capabilities.switch.ID] = {
      [capabilities.switch.commands.on.NAME] = function(self, device, command)
        set_switch(self, device, command, "on")
      end,
      [capabilities.switch.commands.off.NAME] = function(self, device, command)
        set_switch(self, device, command, "off")
      end
    },
    [capabilities.refresh.ID] = {
      [capabilities.refresh.commands.refresh.NAME] = refresh
    }
  }
})

driver:call_on_schedule(60, function()
  for _, device in ipairs(driver:get_devices()) do
    if is_parent(device) then refresh(driver, device) end
  end
end)
driver:call_on_schedule(300, function() find_bridges(driver, false) end)
driver:run()
