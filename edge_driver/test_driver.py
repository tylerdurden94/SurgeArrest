"""Run the real APC Lua driver against a simulated hub/LAN transport."""
from pathlib import Path
import unittest
from lupa import LuaRuntime

HERE = Path(__file__).resolve().parent
STUBS = r'''
events, fields, timers, scheduled, warnings, requests, sockets, wire, timeouts = {}, {}, {}, {}, {}, {}, {}, {}, {}
reading={switches={},commands={}}
failures,post_status=0,202
local function capability(id)
  return setmetatable({ID=id,commands=setmetatable({}, {__index=function(_,k) return {NAME=k} end})},
    {__index=function(_,attr) return function(value) return {cap=id,attr=attr,value=value} end end})
end
package.preload['st.capabilities']=function()
  return setmetatable({}, {__index=function(t,k) local c=capability(k); rawset(t,k,c); return c end}) end
package.preload['log']=function() return {warn=function(v) warnings[#warnings+1]=v end} end
package.preload['st.mdns']=function() return {discover=function(service)
  discovered_service=service;return discovery_result or {found={}} end} end
package.preload['st.json']=function() return {decode=function() return reading end} end
package.preload['ltn12']=function() return {sink={table=function(t) return function(s) t[#t+1]=s end end}} end
package.preload['cosock']=function() return {socket={tcp=function()
  local s={settimeout=function(_,v) timeouts[#timeouts+1]=v end,
    send=function(_,data,first,last)
      wire[#wire+1]=data:sub(first or 1,last or -1)
      if send_error then return nil,send_error,send_partial or 0 end
      return last or #data
    end}
  sockets[#sockets+1]=s; return s end},asyncify=function()
    return {request=function(r)
      requests[#requests+1]={method=r.method,url=r.url}
      local s=r.create(); s:settimeout(5)
      local line=r.method..' '..r.url:match('http://[^/]+(/.*)')..' HTTP/1.1\r\n'
      assert(s:send(line)==#line)
      local header='Host: localhost\r\n'
      for k,v in pairs(r.headers) do header=header..k..': '..v..'\r\n' end
      header=header..'\r\n'
      local sent,err=s:send(header)
      if not sent then return nil,err end
      assert(sent==#header)
      if r.method=='POST' then
        if during_post then local f=during_post;during_post=nil;f() end
        if failures>0 then failures=failures-1;return nil,'timeout' end
        return 1,post_status
      end
      r.sink('json');return 1,200
    end}
  end} end
package.preload['st.driver']=function() return function(_,config)
  driver=config
  function driver:call_with_delay(_,f) timers[#timers+1]=f end
  function driver:call_on_schedule(t,f) scheduled[t]=f end
  function driver:get_devices() return {parent,child} end
  function driver:try_create_device(metadata) creates=creates or {};creates[#creates+1]=metadata end
  function driver:run() end
  return driver end end
parent={id='parent',device_network_id='test',preferences={apcToken=string.rep('x',32)},
  profile={components={main={id='main'},outlet2={id='outlet2'},usb_charger2={id='usb_charger2'}}}}
child={id='child',profile={components={main={id='main'}}}}
function parent:component_exists(k) return self.profile.components[k]~=nil end
function child:component_exists(k) return self.profile.components[k]~=nil end
function parent:get_child_by_parent_assigned_key(k) if k=='outlet1' then return child end end
function child:get_parent_device() return parent end
function parent:get_field(k) return fields[k] end
function parent:set_field(k,v) fields[k]=v end
function parent:online() end
function child:online() end
function parent:offline() end
function parent:emit_component_event(c,e) events[c.id]=e.value end
function child:emit_event(e) events.child=e.value end
fields.bridge_ip,fields.bridge_port='127.0.0.1',80
function command(value) driver.capability_handlers.switch[value](driver,child,{component='main'}) end
function refresh() driver.capability_handlers.refresh.refresh(driver,child,{}) end
function timer() local f=table.remove(timers,1);if f then f() end end
'''


class DriverTests(unittest.TestCase):
    def setUp(self):
        self.lua = LuaRuntime(unpack_returned_tuples=True)
        self.lua.execute(STUBS)
        self.lua.execute((HERE / 'src/init.lua').read_text())

    def test_child_routes_to_outlet1_without_optimistic_event(self):
        self.lua.execute("command('on');assert(requests[1].url:match('/outlet1/on$'));assert(next(events)==nil)")

    def test_observed_state_updates_parent_and_child(self):
        self.lua.execute("reading.switches.outlet1=false;refresh();assert(events.main=='off' and events.child=='off')")

    def test_unknown_state_does_not_emit_requested_state(self):
        self.lua.execute("reading.commands.outlet1={id=1,status='unconfirmed',requested=true,attempts=3};refresh();assert(next(events)==nil);assert(#warnings==1);refresh();assert(#warnings==1)")

    def test_retry_recovers_at_same_address(self):
        self.lua.execute("failures=1;command('on');timer();assert(#requests==2);assert(requests[2].url==requests[1].url)")

    def test_retry_is_bounded(self):
        self.lua.execute("failures=5;command('on');timer();timer();assert(#requests==3 and #timers==0)")

    def test_new_off_cancels_old_on_retry(self):
        self.lua.execute("failures=1;command('on');command('off');timer();assert(#requests==2 and requests[2].url:match('/off$'))")

    def test_superseded_inflight_reply_does_not_schedule_retry(self):
        self.lua.execute("during_post=function() command('off') end;post_status=503;command('on');assert(#requests==2 and #timers==1);timer();assert(requests[3].url:match('/off$'))")

    def test_authentication_errors_not_retried(self):
        self.lua.execute("post_status=401;command('on');assert(#requests==1 and #timers==0)")

    def test_timeout_and_single_write_include_token(self):
        self.lua.execute("command('on');assert(#wire==1);assert(wire[1]:match('X%-APC%-Token:'));for _,v in ipairs(timeouts) do assert(v==60) end")

    def test_combined_send_failure_is_propagated(self):
        self.lua.execute("send_error='closed';send_partial=10;command('on');assert(#timers==1);assert(warnings[1]:match('closed'))")

    def test_children_creation_handles_unknown_state(self):
        self.lua.execute("driver.lifecycle_handlers.added(driver,parent)")

    def test_discovery_uses_standalone_service(self):
        self.lua.execute("driver.discovery(driver,{});assert(discovered_service=='_surgearrest._tcp')")

    def test_discovery_requires_configured_bridge(self):
        self.lua.execute("""
          discovery_result={found={{host_info={address='127.0.0.2',port=80}}}}
          reading={apiVersion=1,bridgeId='example',apc={configured=false,switches={'a','b','c'}}}
          driver.discovery(driver,{});assert(creates==nil)
          reading.apc.configured=true
          driver.discovery(driver,{});assert(#creates==1 and creates[1].label=='APC SurgeArrest')
        """)

    def test_child_labels_are_generic(self):
        self.lua.execute("""
          function parent:get_child_by_parent_assigned_key() return nil end
          driver.lifecycle_handlers.added(driver,parent)
          assert(#creates==6 and creates[1].label=='APC Outlet 1')
          assert(creates[6].parent_assigned_child_key=='led')
        """)

    def test_missing_token_does_not_send_command(self):
        self.lua.execute("parent.preferences.apcToken='';command('on');assert(#requests==0)")

    def test_parent_led_routes_to_led(self):
        self.lua.execute("driver.capability_handlers.switch.off(driver,parent,{component='led'});assert(requests[1].url:match('/led/off$'))")

    def test_invalid_observation_is_not_switch_state(self):
        self.lua.execute("reading.switches.outlet1='on';refresh();assert(next(events)==nil)")


if __name__ == '__main__':
    unittest.main()
