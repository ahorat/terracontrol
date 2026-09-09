#include "WebPortal.h"
#include "WebAssets.h"
#include <AsyncJson.h>
#include <ArduinoJson.h>

static String formatDateTime(const DateTime &dt) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
           dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second());
  return String(buf);
}

static void channelConfigToJson(const ChannelConfig &c, JsonObject obj) {
  obj["mode"] = (int)c.mode;
  obj["weekdayMask"] = c.weekdayMask;
  obj["windowEnabled"] = c.windowEnabled;
  obj["windowStartMin"] = c.windowStartMin;
  obj["windowEndMin"] = c.windowEndMin;
  obj["intervalMinutes"] = c.intervalMinutes;
  obj["onSeconds"] = c.onSeconds;
  obj["fixedOnMin"] = c.fixedOnMin;
  obj["fixedOffMin"] = c.fixedOffMin;
  obj["sunriseOffsetMin"] = c.sunriseOffsetMin;
  obj["sunsetOffsetMin"] = c.sunsetOffsetMin;
  obj["onAtSunrise"] = c.onAtSunrise;
}

static ChannelConfig channelConfigFromJson(JsonVariantConst obj, const ChannelConfig &fallback) {
  ChannelConfig c = fallback;
  c.mode = (ChannelMode)(obj["mode"] | (int)fallback.mode);
  c.weekdayMask = obj["weekdayMask"] | fallback.weekdayMask;
  c.windowEnabled = obj["windowEnabled"] | fallback.windowEnabled;
  c.windowStartMin = obj["windowStartMin"] | fallback.windowStartMin;
  c.windowEndMin = obj["windowEndMin"] | fallback.windowEndMin;
  c.intervalMinutes = obj["intervalMinutes"] | fallback.intervalMinutes;
  c.onSeconds = obj["onSeconds"] | fallback.onSeconds;
  c.fixedOnMin = obj["fixedOnMin"] | fallback.fixedOnMin;
  c.fixedOffMin = obj["fixedOffMin"] | fallback.fixedOffMin;
  c.sunriseOffsetMin = obj["sunriseOffsetMin"] | fallback.sunriseOffsetMin;
  c.sunsetOffsetMin = obj["sunsetOffsetMin"] | fallback.sunsetOffsetMin;
  c.onAtSunrise = obj["onAtSunrise"] | fallback.onAtSunrise;
  return c;
}

void WebPortal::begin(RelayChannel channels[CHANNEL_COUNT], RtcClock *rtc, EepromStore *store, WifiController *net) {
  _channels = channels;
  _rtc = rtc;
  _store = store;
  _net = net;
  setupRoutes();
  _server.begin();
}

static AsyncCallbackJsonWebHandler *jsonPostHandler(const char *uri, ArJsonRequestHandlerFunction fn) {
  auto *h = new AsyncCallbackJsonWebHandler(uri, fn);
  h->setMethod(HTTP_POST);
  return h;
}

void WebPortal::setupRoutes() {
  _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", INDEX_HTML);
  });

  // Captive-portal fallback: any unknown path serves the SPA so phones that
  // auto-open a browser on AP join land on the setup UI.
  _server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(200, "text/html", INDEX_HTML);
  });

  _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) { handleStatus(request); });
  _server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request) { handleGetConfig(request); });
  _server.on("/api/wifi", HTTP_GET, [this](AsyncWebServerRequest *request) { handleGetWifiStatus(request); });
  _server.on("/api/time", HTTP_GET, [this](AsyncWebServerRequest *request) { handleGetTime(request); });

  _server.addHandler(jsonPostHandler("/api/config", [this](AsyncWebServerRequest *request, JsonVariant &json) {
    int index = json["index"] | -1;
    if (index < 0 || index >= CHANNEL_COUNT) {
      request->send(400, "application/json", "{\"error\":\"invalid index\"}");
      return;
    }
    ChannelConfig updated = channelConfigFromJson(json, _channels[index].config());
    _channels[index].setConfig(updated);
    _store->saveChannel(index, updated);
    request->send(200, "application/json", "{\"ok\":true}");
  }));

  _server.addHandler(jsonPostHandler("/api/override", [this](AsyncWebServerRequest *request, JsonVariant &json) {
    int index = json["index"] | -1;
    String action = json["action"] | "";
    if (index < 0 || index >= CHANNEL_COUNT) {
      request->send(400, "application/json", "{\"error\":\"invalid index\"}");
      return;
    }
    DateTime now = _rtc->now();
    if (action == "on" || action == "off") {
      uint32_t duration = json["durationSeconds"] | 300;
      _channels[index].setOverride(action == "on", duration, now);
    } else if (action == "clear") {
      _channels[index].clearOverride();
    } else {
      request->send(400, "application/json", "{\"error\":\"invalid action\"}");
      return;
    }
    request->send(200, "application/json", "{\"ok\":true}");
  }));

  _server.addHandler(jsonPostHandler("/api/wifi", [this](AsyncWebServerRequest *request, JsonVariant &json) {
    if (json["forceAp"] | false) {
      _net->forceApMode();
      request->send(200, "application/json", "{\"ok\":true}");
      return;
    }
    String ssid = json["ssid"] | "";
    String password = json["password"] | "";
    if (ssid.length() == 0) {
      request->send(400, "application/json", "{\"error\":\"ssid required\"}");
      return;
    }
    _net->applyNewCredentials(ssid, password);
    request->send(200, "application/json", "{\"ok\":true}");
  }));

  _server.addHandler(jsonPostHandler("/api/time", [this](AsyncWebServerRequest *request, JsonVariant &json) {
    int year = json["year"] | -1;
    int month = json["month"] | -1;
    int day = json["day"] | -1;
    int hour = json["hour"] | 0;
    int minute = json["minute"] | 0;
    int second = json["second"] | 0;
    if (year < 2000 || month < 1 || month > 12 || day < 1 || day > 31) {
      request->send(400, "application/json", "{\"error\":\"invalid date\"}");
      return;
    }
    DateTime dt(year, month, day, hour, minute, second);
    _rtc->setLocal(dt);
    request->send(200, "application/json", "{\"ok\":true}");
  }));
}

void WebPortal::handleStatus(AsyncWebServerRequest *request) {
  JsonDocument doc;
  bool valid = _rtc->isTimeValid();
  doc["rtcValid"] = valid;
  DateTime now = _rtc->now();
  doc["now"] = formatDateTime(now);
  long sunOffset = RtcClock::localUtcOffsetSeconds(now);

  JsonArray arr = doc["channels"].to<JsonArray>();
  for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["index"] = i;
    o["on"] = _channels[i].isOn();
    o["mode"] = (int)_channels[i].config().mode;
    o["modeName"] = _channels[i].modeName();

    DateTime next = _channels[i].nextScheduledTransition(now, sunOffset);
    o["nextSwitch"] = next.unixtime() == 0 ? "" : formatDateTime(next);

    JsonObject ov = o["override"].to<JsonObject>();
    ov["active"] = _channels[i].overrideActive();
    ov["indefinite"] = _channels[i].overrideIndefinite();
    ov["state"] = _channels[i].isOn();
    ov["endsAt"] = _channels[i].overrideActive() && !_channels[i].overrideIndefinite()
                       ? formatDateTime(_channels[i].overrideEndsAt())
                       : "";
  }

  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

void WebPortal::handleGetConfig(AsyncWebServerRequest *request) {
  JsonDocument doc;
  JsonArray arr = doc["channels"].to<JsonArray>();
  for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["index"] = i;
    channelConfigToJson(_channels[i].config(), o);
  }
  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

void WebPortal::handleGetWifiStatus(AsyncWebServerRequest *request) {
  JsonDocument doc;
  doc["mode"] = _net->mode() == WifiController::Mode::AP ? "AP" : "STA";
  doc["connected"] = _net->staConnected();
  doc["summary"] = _net->statusSummary();
  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

void WebPortal::handleGetTime(AsyncWebServerRequest *request) {
  JsonDocument doc;
  bool valid = _rtc->isTimeValid();
  doc["rtcValid"] = valid;
  doc["now"] = formatDateTime(_rtc->now());
  doc["ntpLastSync"] = _net->ntpEverSynced()
      ? formatDateTime(RtcClock::utcToLocal(DateTime((uint32_t)_net->lastNtpSyncEpoch())))
      : "";
  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}
