#pragma once

#include <ESPAsyncWebServer.h>
#include "RelayChannel.h"
#include "WifiController.h"
#include "RtcClock.h"
#include "EepromStore.h"

// Serves the embedded web UI and its JSON API. Owns no scheduling logic
// itself - it only reads/writes the ChannelConfig array (persisting via
// EepromStore) and reads status from the RelayChannel/RtcClock/WifiController
// it is given.
class WebPortal {
public:
  void begin(RelayChannel channels[CHANNEL_COUNT], RtcClock *rtc, EepromStore *store,
             WifiController *net);

private:
  AsyncWebServer _server{80};
  RelayChannel *_channels = nullptr;
  RtcClock *_rtc = nullptr;
  EepromStore *_store = nullptr;
  WifiController *_net = nullptr;

  void setupRoutes();

  // POST handlers (/api/config, /api/override, /api/wifi, /api/time) are
  // registered directly as lambdas in setupRoutes() via AsyncCallbackJsonWebHandler.
  void handleStatus(AsyncWebServerRequest *request);
  void handleGetConfig(AsyncWebServerRequest *request);
  void handleGetWifiStatus(AsyncWebServerRequest *request);
  void handleGetTime(AsyncWebServerRequest *request);
};
