#include "apc_lan.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <cJSON.h>
#include <esp_timer.h>
#include <mbedtls/aes.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>

namespace {
constexpr char kNames[][16] = {
    "outlet1", "outlet2", "outlet3", "usb_charger1", "usb_charger2", "led"};

int propertyIndex(const String& name) {
  for (int i = 0; i < 6; ++i) if (name == kNames[i]) return i;
  return -1;
}

bool hmacSha256(const uint8_t* key, size_t keyLength, const uint8_t* data,
                size_t dataLength, uint8_t output[32]) {
  const auto* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  return md && mbedtls_md_hmac(md, key, keyLength, data, dataLength, output) == 0;
}

String b64(const uint8_t* data, size_t length) {
  unsigned char output[1024] = {};
  size_t used = 0;
  if (mbedtls_base64_encode(output, sizeof(output) - 1, &used, data, length) != 0)
    return String();
  output[used] = 0;
  return String(reinterpret_cast<const char*>(output));
}

String decimal(uint64_t value) {
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(value));
  return String(buffer);
}

String jsonString(const String& value) {
  String result = "\"";
  for (char c : value) {
    if (c == '\"' || c == '\\') result += '\\';
    if (static_cast<uint8_t>(c) >= 0x20) result += c;
  }
  return result + '\"';
}
}  // namespace

void ApcLan::begin() {
  Preferences prefs;
  if (!prefs.begin("apcbridge", true)) return;
  stripIp_ = prefs.getString("stripip", "");
  lanKey_ = prefs.getString("lankey", "");
  apiToken_ = prefs.getString("apitoken", "");
  keyId_ = prefs.getUInt("keyid", 0);
  prefs.end();
  IPAddress parsed;
  configured_ = parsed.fromString(stripIp_) && lanKey_.length() == 32 &&
    apiToken_.length() >= 24 && keyId_ != 0;
  clearSession();
}

bool ApcLan::provision(const String& ip, uint32_t keyId, const String& lanKey,
                       const String& apiToken) {
  IPAddress parsed;
  if (!parsed.fromString(ip) || keyId == 0 || lanKey.length() != 32 ||
      apiToken.length() < 24 || apiToken.length() > 64) return false;
  for (size_t i = 0; i < lanKey.length(); ++i)
    if (static_cast<uint8_t>(lanKey[i]) >= 128) return false;
  for (size_t i = 0; i < apiToken.length(); ++i)
    if (!isalnum(static_cast<unsigned char>(apiToken[i])) &&
        apiToken[i] != '-' && apiToken[i] != '_') return false;
  Preferences prefs;
  if (!prefs.begin("apcbridge", false)) return false;
  const bool okay = prefs.putString("stripip", ip) == ip.length() &&
    prefs.putString("lankey", lanKey) == lanKey.length() &&
    prefs.putString("apitoken", apiToken) == apiToken.length() &&
    prefs.putUInt("keyid", keyId) == sizeof(keyId);
  prefs.end();
  if (!okay) return false;
  stripIp_ = ip;
  lanKey_ = lanKey;
  apiToken_ = apiToken;
  keyId_ = keyId;
  configured_ = true;
  clearSession();
  lastRegisterMs_ = 0;
  return true;
}

bool ApcLan::authorized(WebServer& server) const {
  if (!configured_ || server.header("X-APC-Token") != apiToken_) {
    server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return false;
  }
  return true;
}

void ApcLan::clearSession() {
  sessionReady_ = false;
  sequence_ = 1;
  memset(appSignKey_, 0, sizeof(appSignKey_));
  memset(appCryptoKey_, 0, sizeof(appCryptoKey_));
  memset(appIv_, 0, sizeof(appIv_));
  memset(devSignKey_, 0, sizeof(devSignKey_));
  memset(devCryptoKey_, 0, sizeof(devCryptoKey_));
  memset(devIv_, 0, sizeof(devIv_));
}

bool ApcLan::fromStrip(WebServer& server) const {
  IPAddress expected;
  return configured_ && expected.fromString(stripIp_) &&
         server.client().remoteIP() == expected;
}

void ApcLan::derive(uint8_t suffix, bool appDirection, uint8_t output[32]) const {
  const String material = appDirection
    ? random1_ + random2_ + time1_ + time2_ + char('0' + suffix)
    : random2_ + random1_ + time2_ + time1_ + char('0' + suffix);
  uint8_t first[32] = {};
  hmacSha256(reinterpret_cast<const uint8_t*>(lanKey_.c_str()), lanKey_.length(),
             reinterpret_cast<const uint8_t*>(material.c_str()), material.length(), first);
  uint8_t secondInput[128] = {};
  if (sizeof(first) + material.length() > sizeof(secondInput)) return;
  memcpy(secondInput, first, sizeof(first));
  memcpy(secondInput + sizeof(first), material.c_str(), material.length());
  hmacSha256(reinterpret_cast<const uint8_t*>(lanKey_.c_str()), lanKey_.length(),
             secondInput, sizeof(first) + material.length(), output);
}

void ApcLan::handleKeyExchange(WebServer& server) {
  if (!fromStrip(server) || server.arg("plain").length() > 512) {
    server.send(403, "application/json", "{}");
    return;
  }
  cJSON* root = cJSON_Parse(server.arg("plain").c_str());
  cJSON* exchange = root ? cJSON_GetObjectItemCaseSensitive(root, "key_exchange") : nullptr;
  cJSON* keyId = exchange ? cJSON_GetObjectItemCaseSensitive(exchange, "key_id") : nullptr;
  cJSON* rnd = exchange ? cJSON_GetObjectItemCaseSensitive(exchange, "random_1") : nullptr;
  cJSON* timestamp = exchange ? cJSON_GetObjectItemCaseSensitive(exchange, "time_1") : nullptr;
  if (!cJSON_IsNumber(keyId) || keyId->valuedouble != keyId_ ||
      !cJSON_IsString(rnd) || !cJSON_IsNumber(timestamp) ||
      strlen(rnd->valuestring) != 16 || timestamp->valuedouble < 0) {
    cJSON_Delete(root);
    server.send(400, "application/json", "{}");
    return;
  }
  clearSession();
  random1_ = rnd->valuestring;
  time1_ = decimal(static_cast<uint64_t>(timestamp->valuedouble));
  random2_ = "";
  static const char alphabet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  for (int i = 0; i < 16; ++i) random2_ += alphabet[esp_random() % 62];
  time2_ = decimal(static_cast<uint64_t>(esp_timer_get_time()));
  cJSON_Delete(root);
  uint8_t ivSeed[32] = {};
  derive(0, true, appSignKey_);
  derive(1, true, appCryptoKey_);
  derive(2, true, ivSeed);
  memcpy(appIv_, ivSeed, sizeof(appIv_));
  derive(0, false, devSignKey_);
  derive(1, false, devCryptoKey_);
  derive(2, false, ivSeed);
  memcpy(devIv_, ivSeed, sizeof(devIv_));
  sessionReady_ = true;
  freshRegistration_ = false;
  lastCallbackMs_ = millis();
  ++keyExchanges_;
  // A new session invalidates outstanding read IDs, not the owner's target.
  for (int i = 0; i < 6; ++i) reads_[i] = Read();
  refreshStates();
  Serial.printf("APC session established (%u)\n", keyExchanges_);
  server.send(200, "application/json", "{\"random_2\":" + jsonString(random2_) +
              ",\"time_2\":" + time2_ + "}");
}

bool ApcLan::encryptAndSign(const String& plain, String& enc, String& sign) {
  if (!sessionReady_ || plain.length() > 496) return false;
  uint8_t signature[32] = {};
  if (!hmacSha256(appSignKey_, sizeof(appSignKey_),
                  reinterpret_cast<const uint8_t*>(plain.c_str()), plain.length(), signature))
    return false;
  const size_t padded = (plain.length() + 15) & ~size_t(15);
  uint8_t input[512] = {};
  uint8_t output[512] = {};
  memcpy(input, plain.c_str(), plain.length());
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  const int result = mbedtls_aes_setkey_enc(&aes, appCryptoKey_, 256) == 0
    ? mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padded, appIv_, input, output)
    : -1;
  mbedtls_aes_free(&aes);
  if (result != 0) return false;
  enc = b64(output, padded);
  sign = b64(signature, sizeof(signature));
  return !enc.isEmpty() && !sign.isEmpty();
}

void ApcLan::handleCommands(WebServer& server) {
  if (!fromStrip(server) || !sessionReady_) {
    server.send(403, "application/json", "{}");
    return;
  }
  const uint32_t now = millis();
  int write = -1, read = -1;
  for (int i = 0; i < 6; ++i) {
    // Only expire an already queued write here. Recovery/retry transitions
    // belong to tick(), which re-keys before the next delivery attempt.
    if (deliveries_[i].phase == ApcDelivery::Queued) deliveries_[i].tick(now);
    if (deliveries_[i].phase == ApcDelivery::Queued &&
        (write < 0 || deliveries_[i].id < deliveries_[write].id)) write = i;
  }
  if (write < 0) {
    for (int i = 0; i < 6; ++i)
      if (reads_[i].queued) { read = i; break; }
  }
  String plain = "{\"seq_no\":" + String(sequence_) + ",\"data\":{";
  if (write >= 0) {
    plain += "\"properties\":[{\"property\":{\"base_type\":\"boolean\",\"value\":" +
      String(deliveries_[write].target) + ",\"metadata\":null,\"name\":" +
      jsonString(kNames[write]) + ",\"id\":" + jsonString(String(deliveries_[write].id)) + "}}]";
  } else if (read >= 0) {
    plain += "\"cmds\":[{\"cmd\":{\"method\":\"GET\",\"resource\":" +
      jsonString("property.json?name=" + String(kNames[read])) +
      ",\"uri\":\"/local_lan/property/datapoint.json\",\"data\":\"\",\"cmd_id\":" +
      String(nextReadId_) + "}}]";
  }
  plain += "}}";
  String enc, sign;
  if (!encryptAndSign(plain, enc, sign)) {
    server.send(500, "application/json", "{}");
    return;
  }
  // Sending the ciphertext is a delivery attempt, never physical confirmation.
  bool more = write >= 0;  // A readback is queued for each write below.
  for (int i = 0; i < 6; ++i)
    if (i != write && deliveries_[i].phase == ApcDelivery::Queued) more = true;
  for (int i = 0; i < 6; ++i)
    if (i != read && reads_[i].queued) more = true;
  server.send(more ? 206 : 200, "application/json", "{\"enc\":" + jsonString(enc) +
              ",\"sign\":" + jsonString(sign) + "}");
  ++sequence_;
  lastCallbackMs_ = now;
  if (write >= 0) {
    deliveries_[write].sent(now);
    requestRead(write, deliveries_[write].id);
    Serial.printf("APC %s request %u sent, attempt %u/%u; awaiting confirmation\n",
      kNames[write], deliveries_[write].id, deliveries_[write].attempts,
      ApcDelivery::kMaxAttempts);
  } else if (read >= 0) {
    reads_[read].queued = false;
    reads_[read].id = nextReadId_++;
    reads_[read].issuedAt = now;
    ++reads_[read].attempts;
  }
}

bool ApcLan::decryptAndVerify(const String& body, String& plain) {
  if (!sessionReady_ || body.length() > 2048) return false;
  cJSON* root = cJSON_Parse(body.c_str());
  cJSON* enc = root ? cJSON_GetObjectItemCaseSensitive(root, "enc") : nullptr;
  cJSON* sign = root ? cJSON_GetObjectItemCaseSensitive(root, "sign") : nullptr;
  uint8_t input[1024] = {}, output[1025] = {}, signature[32] = {}, expected[32] = {};
  size_t length = 0, signLength = 0;
  bool okay = cJSON_IsString(enc) && cJSON_IsString(sign) &&
    mbedtls_base64_decode(input, sizeof(input), &length,
      reinterpret_cast<const uint8_t*>(enc->valuestring), strlen(enc->valuestring)) == 0 &&
    length > 0 && length % 16 == 0 &&
    mbedtls_base64_decode(signature, sizeof(signature), &signLength,
      reinterpret_cast<const uint8_t*>(sign->valuestring), strlen(sign->valuestring)) == 0 &&
    signLength == sizeof(signature);
  cJSON_Delete(root);
  if (!okay) return false;
  uint8_t iv[16];
  memcpy(iv, devIv_, sizeof(iv));
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  okay = mbedtls_aes_setkey_dec(&aes, devCryptoKey_, 256) == 0 &&
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, length, iv, input, output) == 0;
  mbedtls_aes_free(&aes);
  if (!okay) return false;
  while (length && output[length - 1] == 0) --length;
  if (!hmacSha256(devSignKey_, sizeof(devSignKey_), output, length, expected)) return false;
  uint8_t difference = 0;
  for (size_t i = 0; i < sizeof(signature); ++i) difference |= signature[i] ^ expected[i];
  if (difference) return false;
  // Invalid signatures must not advance CBC state. Valid messages do, even if
  // their JSON shape is unknown to this firmware.
  memcpy(devIv_, iv, sizeof(devIv_));
  output[length] = 0;
  plain = String(reinterpret_cast<const char*>(output));
  return plain.length() == length;
}

void ApcLan::handleDatapoint(WebServer& server) {
  if (!fromStrip(server)) {
    server.send(403, "application/json", "{}");
    return;
  }
  const uint32_t cmdId = static_cast<uint32_t>(strtoul(server.arg("cmd_id").c_str(), nullptr, 10));
  const int status = server.arg("status").toInt();
  if (server.arg("plain").isEmpty()) {
    // Older modules answer an unsupported read with an empty error callback.
    // Acknowledge it to prevent an endless callback retry; never call it state.
    for (int i = 0; i < 6; ++i) {
      if (cmdId && reads_[i].id == cmdId && status >= 400) {
        lastError_ = "state read rejected: HTTP " + String(status);
        ++callbackErrors_;
        reads_[i].id = 0;
        Serial.printf("APC %s state read rejected: HTTP %d\n", kNames[i], status);
      }
    }
    server.send(haveWork() ? 206 : 200, "application/json", "{}");
    return;
  }
  String plain;
  if (!decryptAndVerify(server.arg("plain"), plain)) {
    ++callbackErrors_;
    lastError_ = "invalid callback signature or session";
    freshRegistration_ = true;
    notifyDue_ = true;
    server.send(400, "application/json", "{}");
    return;
  }
  lastCallbackMs_ = millis();
  cJSON* root = cJSON_Parse(plain.c_str());
  cJSON* data = root ? cJSON_GetObjectItemCaseSensitive(root, "data") : nullptr;
  cJSON* name = data ? cJSON_GetObjectItemCaseSensitive(data, "name") : nullptr;
  cJSON* value = data ? cJSON_GetObjectItemCaseSensitive(data, "value") : nullptr;
  const int index = cJSON_IsString(name) ? propertyIndex(name->valuestring) : -1;
  const bool validValue = cJSON_IsBool(value) ||
    (cJSON_IsNumber(value) && (value->valuedouble == 0 || value->valuedouble == 1));
  if (index >= 0 && validValue && (status == 0 || (status >= 200 && status < 300))) {
    const uint8_t observed = cJSON_IsTrue(value) || (cJSON_IsNumber(value) && value->valueint == 1);
    const Read& read = reads_[index];
    // An older read must not overwrite a newer request or confirm its target.
    const bool currentRead = cmdId != 0 && read.id == cmdId &&
      (read.generation == 0 ? !deliveries_[index].pending() : read.generation == deliveries_[index].id);
    if (cmdId == 0 || currentRead) {
      values_[index] = observed;
      valueAt_[index] = millis();
      if (currentRead && read.generation != 0 &&
          deliveries_[index].confirm(read.generation, observed)) {
        lastError_ = "";
        Serial.printf("APC %s request %u confirmed by state read\n", kNames[index], read.generation);
      }
    }
    if (currentRead) reads_[index].id = 0;
  }
  cJSON_Delete(root);
  // This is an HTTP acknowledgement, not a new encrypted command. Echoing a
  // device-direction ciphertext can make the module reject it and re-key.
  server.send(haveWork() ? 206 : 200, "application/json", "{}");
}

bool ApcLan::registerLocal(uint8_t notify) {
  if (!configured_ || WiFi.status() != WL_CONNECTED) return false;
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(2500);
  if (!http.begin(client, "http://" + stripIp_ + "/local_reg.json")) return false;
  http.addHeader("Content-Type", "application/json");
  const String body = "{\"local_reg\":{\"uri\":\"/local_lan\",\"notify\":" +
    String(notify) + ",\"ip\":\"" + WiFi.localIP().toString() +
    "\",\"port\":8080}}";
  if (freshRegistration_) clearSession();
  const int status = freshRegistration_ ? http.POST(body) : http.PUT(body);
  http.end();
  lastRegisterMs_ = millis();
  lastRegisterStatus_ = status;
  if (status != 202) lastError_ = "registration failed: " + String(status);
  if (status >= 400) freshRegistration_ = true;
  return status == 202;
}

bool ApcLan::haveWork() const {
  for (int i = 0; i < 6; ++i)
    if (deliveries_[i].phase == ApcDelivery::Queued || reads_[i].queued) return true;
  return false;
}

void ApcLan::requestRead(int index, uint32_t generation) {
  reads_[index] = Read();
  reads_[index].queued = true;
  reads_[index].generation = generation;
  notifyDue_ = true;
}

void ApcLan::refreshStates() {
  for (int i = 0; i < 6; ++i)
    requestRead(i, deliveries_[i].pending() ? deliveries_[i].id : 0);
  lastRefreshMs_ = millis();
}

void ApcLan::tick() {
  if (!configured_ || WiFi.status() != WL_CONNECTED) return;
  const uint32_t now = millis();
  for (int i = 0; i < 6; ++i) {
    const auto prior = deliveries_[i].phase;
    if (deliveries_[i].tick(now)) {
      // Lost ciphertext can desynchronize CBC for the entire session. Re-key
      // before resending instead of indefinitely using stale session keys.
      freshRegistration_ = true;
      notifyDue_ = true;
      reads_[i] = Read();
    }
    if (prior != deliveries_[i].phase && deliveries_[i].phase == ApcDelivery::Unconfirmed) {
      lastError_ = String(kNames[i]) + " request " + String(deliveries_[i].id) + " unconfirmed";
      Serial.printf("APC %s request %u unconfirmed after %u attempts; retries stopped\n",
        kNames[i], deliveries_[i].id, deliveries_[i].attempts);
    }
  }
  if (now - lastRefreshMs_ >= 30000) refreshStates();
  if (haveWork() && sessionReady_ && now - lastCallbackMs_ >= 30000) {
    freshRegistration_ = true;
    notifyDue_ = true;
  }
  const uint32_t interval = haveWork() ? 3000 : 10000;
  if (lastRegisterMs_ == 0 ||
      (notifyDue_ && now - lastRegisterMs_ >= 300) ||
      now - lastRegisterMs_ >= interval) {
    // Pace retries when a registration failed, even if work remains queued.
    if (lastRegisterMs_ && lastRegisterStatus_ != 202 && now - lastRegisterMs_ < 3000) return;
    notifyDue_ = false;
    registerLocal(haveWork() ? 1 : 0);
  }
}

bool ApcLan::queue(const String& name, uint8_t value) {
  const int index = propertyIndex(name);
  if (!configured_ || index < 0 || value > 1) return false;
  deliveries_[index].queue(nextRequestId_++, value, millis());
  reads_[index] = Read();
  // A previous observation is no longer evidence for this new target.
  values_[index] = -1;
  notifyDue_ = true;
  Serial.printf("APC %s request %u queued: %s\n", kNames[index], deliveries_[index].id,
    value ? "on" : "off");
  // The HTTP handler accepts locally, then tick() notifies after its response.
  // This avoids waiting on strip networking while the hub's request is open.
  return true;
}

void ApcLan::handleApi(WebServer& server) {
  const String path = server.uri();
  if (path == "/v1/apc" && server.method() == HTTP_GET) {
    server.send(200, "application/json", statusJson());
    return;
  }
  if (path == "/v1/apc/refresh" && server.method() == HTTP_POST) {
    refreshStates();
    server.send(202, "application/json", "{\"queued\":true}");
    return;
  }
  if (!path.startsWith("/v1/apc/") || server.method() != HTTP_POST) {
    server.send(404, "application/json", "{}");
    return;
  }
  const String rest = path.substring(strlen("/v1/apc/"));
  const int slash = rest.indexOf('/');
  const String name = slash >= 0 ? rest.substring(0, slash) : "";
  const String action = slash >= 0 ? rest.substring(slash + 1) : "";
  if (propertyIndex(name) < 0 || (action != "on" && action != "off")) {
    server.send(400, "application/json", "{\"error\":\"invalid switch\"}");
    return;
  }
  if (!queue(name, action == "on" ? 1 : 0)) {
    server.send(503, "application/json", "{\"error\":\"APC command unavailable\"}");
    return;
  }
  server.send(202, "application/json", "{\"queued\":true}");
}

String ApcLan::infoJson() const {
  String result = "{\"configured\":";
  result += configured_ ? "true" : "false";
  result += ",\"switches\":[";
  if (configured_) {
    for (int i = 0; i < 6; ++i) {
      if (i) result += ',';
      result += jsonString(kNames[i]);
    }
  }
  return result + "]}";
}

String ApcLan::statusJson() const {
  String result = "{\"connected\":";
  result += sessionReady_ && millis() - lastRegisterMs_ < 30000 ? "true" : "false";
  String pending;
  for (int i = 0; i < 6; ++i) if (deliveries_[i].pending()) { pending = kNames[i]; break; }
  result += ",\"pending\":" + jsonString(pending) + ",\"switches\":{";
  for (int i = 0; i < 6; ++i) {
    if (i) result += ',';
    result += jsonString(kNames[i]) + ':';
    result += values_[i] < 0 || millis() - valueAt_[i] >= 90000
      ? "null" : (values_[i] ? "true" : "false");
  }
  result += "},\"commands\":{";
  for (int i = 0; i < 6; ++i) {
    if (i) result += ',';
    const auto& command = deliveries_[i];
    result += jsonString(kNames[i]) + ":{\"id\":" + String(command.id) +
      ",\"requested\":" + (command.id ? (command.target ? "true" : "false") : "null") +
      ",\"status\":" + jsonString(command.status()) + ",\"attempts\":" + String(command.attempts) + "}";
  }
  result += "},\"diagnostics\":{\"lastRegisterStatus\":" + String(lastRegisterStatus_) +
    ",\"keyExchanges\":" + String(keyExchanges_) + ",\"callbackErrors\":" + String(callbackErrors_) +
    ",\"lastError\":" + jsonString(lastError_) + "}";
  return result + "}";
}
