#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <stdint.h>
#include "apc_delivery.h"

class ApcLan {
 public:
  void begin();
  bool provision(const String& ip, uint32_t keyId, const String& lanKey,
                 const String& apiToken);
  bool authorized(WebServer& server) const;
  bool configured() const { return configured_; }
  void tick();
  void handleKeyExchange(WebServer& server);
  void handleCommands(WebServer& server);
  void handleDatapoint(WebServer& server);
  void handleApi(WebServer& server);
  String infoJson() const;
  String statusJson() const;

 private:
  bool fromStrip(WebServer& server) const;
  bool registerLocal(uint8_t notify);
  bool queue(const String& name, uint8_t value);
  bool encryptAndSign(const String& plain, String& enc, String& sign);
  bool decryptAndVerify(const String& body, String& plain);
  bool haveWork() const;
  void requestRead(int index, uint32_t generation);
  void refreshStates();
  void derive(uint8_t suffix, bool appDirection, uint8_t output[32]) const;
  void clearSession();

  bool configured_ = false;
  bool sessionReady_ = false;
  String stripIp_;
  String lanKey_;
  String apiToken_;
  uint32_t keyId_ = 0;
  String random1_, random2_, time1_, time2_;
  uint8_t appSignKey_[32] = {};
  uint8_t appCryptoKey_[32] = {};
  uint8_t appIv_[16] = {};
  uint8_t devSignKey_[32] = {};
  uint8_t devCryptoKey_[32] = {};
  uint8_t devIv_[16] = {};
  uint32_t sequence_ = 1;
  ApcDelivery deliveries_[6];
  uint32_t nextRequestId_ = 1;
  uint32_t nextReadId_ = 1;
  struct Read {
    bool queued = false;
    uint32_t generation = 0;
    uint32_t id = 0;
    uint32_t issuedAt = 0;
    uint8_t attempts = 0;
  } reads_[6];
  int8_t values_[6] = {-1, -1, -1, -1, -1, -1};
  uint32_t valueAt_[6] = {};
  uint32_t lastRegisterMs_ = 0;
  uint32_t lastCallbackMs_ = 0;
  uint32_t lastRefreshMs_ = 0;
  uint32_t keyExchanges_ = 0;
  uint32_t callbackErrors_ = 0;
  int lastRegisterStatus_ = 0;
  String lastError_;
  bool notifyDue_ = false;
  bool freshRegistration_ = true;
};
