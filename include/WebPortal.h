#pragma once

#include <ESPAsyncWebServer.h>
#include "RelayChannel.h"
#include "NetworkManager.h"
#include "RtcClock.h"
#include "EepromStore.h"

// Serves the embedded web UI and its JSON API. Owns no scheduling logic
// itself - it only reads/writes the ChannelConfig array (persisting via
// EepromStore) and reads status from the RelayChannel/RtcClock/NetworkManager
// it is given.
class WebPortal {
public:
  void begin(RelayChannel channels[CHANNEL_COUNT], RtcClock *rtc, EepromStore *store,
             NetworkManager *net);

private:
  AsyncWebServer _server{80};
  RelayChannel *_channels = nullptr;
  RtcClock *_rtc = nullptr;
  EepromStore *_store = nullptr;
  NetworkManager *_net = nullptr;

  void setupRoutes();

  void handleStatus(AsyncWebServerRequest *request);
  void handleGetConfig(AsyncWebServerRequest *request);
  void handlePostConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len);
  void handlePostOverride(AsyncWebServerRequest *request, uint8_t *data, size_t len);
  void handleGetWifiStatus(AsyncWebServerRequest *request);
  void handlePostWifi(AsyncWebServerRequest *request, uint8_t *data, size_t len);
  void handleGetTime(AsyncWebServerRequest *request);
  void handlePostTime(AsyncWebServerRequest *request, uint8_t *data, size_t len);
};
