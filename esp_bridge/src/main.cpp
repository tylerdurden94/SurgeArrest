#include <Arduino.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <cJSON.h>
#include "apc_lan.h"

namespace {
constexpr char kFirmware[] = "surgearrest-0.1.0";
WebServer server(80);
WebServer callbacks(8080);
ApcLan apc;
String ssid, password, input;
String bridgeId;
bool mdnsStarted = false;
bool serialOverflow = false;
uint32_t lastReconnect = 0;

void info(WebServer& target) {
  target.send(200, "application/json", String("{\"apiVersion\":1,\"bridgeId\":\"") +
    bridgeId + "\",\"firmware\":\"" + kFirmware + "\",\"apc\":" + apc.infoJson() + "}");
}

void routes(WebServer& target) {
  const char* headers[] = {"X-APC-Token"};
  target.collectHeaders(headers, 1);
  target.on("/v1/info", HTTP_GET, [&target]() { info(target); });
  target.on("/local_lan/key_exchange.json", HTTP_POST,
    [&target]() { apc.handleKeyExchange(target); });
  target.on("/local_lan/commands.json", HTTP_GET,
    [&target]() { apc.handleCommands(target); });
  target.on("/local_lan/property/datapoint.json", HTTP_POST,
    [&target]() { apc.handleDatapoint(target); });
  target.on("/local_lan/property/datapoint.json", HTTP_PUT,
    [&target]() { apc.handleDatapoint(target); });
  target.on("/local_lan/property/datapoint/ack.json", HTTP_POST,
    [&target]() { apc.handleDatapoint(target); });
  target.onNotFound([&target]() {
    if (target.uri() == "/v1/apc" || target.uri().startsWith("/v1/apc/")) {
      if (apc.authorized(target)) apc.handleApi(target);
    } else {
      target.send(404, "application/json", "{}");
    }
  });
  target.begin();
}

bool saveNetwork(const String& json) {
  cJSON* root = cJSON_Parse(json.c_str());
  cJSON* name = root ? cJSON_GetObjectItemCaseSensitive(root, "ssid") : nullptr;
  cJSON* secret = root ? cJSON_GetObjectItemCaseSensitive(root, "password") : nullptr;
  bool okay = cJSON_IsString(name) && cJSON_IsString(secret) &&
    strlen(name->valuestring) >= 1 && strlen(name->valuestring) <= 32 &&
    strlen(secret->valuestring) <= 63;
  Preferences prefs;
  if (okay) {
    okay = prefs.begin("sa_network", false);
    if (okay) {
      okay = prefs.putString("ssid", name->valuestring) == strlen(name->valuestring) &&
        prefs.putString("password", secret->valuestring) == strlen(secret->valuestring);
      prefs.end();
    }
  }
  cJSON_Delete(root);
  return okay;
}

bool saveApc(const String& command) {
  String parts[5];
  int start = 0;
  for (int i = 0; i < 4; ++i) {
    int end = command.indexOf('|', start);
    if (end < 0) return false;
    parts[i] = command.substring(start, end);
    start = end + 1;
  }
  parts[4] = command.substring(start);
  if (parts[0] != "APC_CONFIG" || parts[2].isEmpty()) return false;
  char* end = nullptr;
  unsigned long keyId = strtoul(parts[2].c_str(), &end, 10);
  if (!end || *end || keyId == 0) return false;
  return apc.provision(parts[1], static_cast<uint32_t>(keyId), parts[3], parts[4]);
}

void serialSetup() {
  while (Serial.available()) {
    char ch = static_cast<char>(Serial.read());
    if (ch == '\r') continue;
    if (ch != '\n') {
      if (input.length() < 512 && !serialOverflow) input += ch;
      else { serialOverflow = true; input = ""; }
      continue;
    }
    if (serialOverflow) Serial.println("CONFIG_ERROR");
    else if (input == "HELLO") Serial.println("SURGEARREST_READY");
    else if (input.startsWith("NETWORK_CONFIG|"))
      Serial.println(saveNetwork(input.substring(15)) ? "NETWORK_CONFIG_OK" : "CONFIG_ERROR");
    else if (input.startsWith("APC_CONFIG|"))
      Serial.println(saveApc(input) ? "APC_CONFIG_OK" : "CONFIG_ERROR");
    else if (input == "RESTART") {
      Serial.println("RESTARTING"); Serial.flush(); delay(100); ESP.restart();
    } else if (!input.isEmpty()) Serial.println("CONFIG_ERROR");
    input = "";
    serialOverflow = false;
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  input.reserve(512);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  bridgeId = WiFi.macAddress();
  Preferences prefs;
  if (prefs.begin("sa_network", true)) {
    ssid = prefs.getString("ssid", ""); password = prefs.getString("password", ""); prefs.end();
  }
  apc.begin();
  if (!ssid.isEmpty()) { WiFi.setAutoReconnect(true); WiFi.begin(ssid.c_str(), password.c_str()); }
  routes(server);
  routes(callbacks);
  Serial.println(kFirmware);
  Serial.println("SURGEARREST_READY");
}

void loop() {
  serialSetup();
  server.handleClient();
  callbacks.handleClient();
  apc.tick();
  if (WiFi.status() == WL_CONNECTED && !mdnsStarted) {
    String host = "surgearrest-" + bridgeId;
    host.replace(":", ""); host.toLowerCase();
    if (MDNS.begin(host.c_str())) { MDNS.addService("surgearrest", "tcp", 80); mdnsStarted = true; }
    Serial.println("Bridge IP: " + WiFi.localIP().toString());
  }
  if (!ssid.isEmpty() && WiFi.status() != WL_CONNECTED && millis() - lastReconnect >= 30000) {
    lastReconnect = millis(); WiFi.reconnect();
  }
  delay(2);
}
