#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include "WifiCredStore.h"

// Owns WiFi AP/STA mode switching, the reset-button-triggered fallback to AP
// mode, the background reconnect timer, and the AP-mode captive portal DNS
// redirect. The relay scheduler never depends on this class's state.
class NetworkManager {
public:
  enum class Mode { AP, STA };

  void begin(WifiCredStore *credStore);

  // Call every loop() iteration; non-blocking, debounces the reset button
  // and drives reconnect attempts in the background.
  void loop();

  Mode mode() const { return _mode; }
  bool staConnected() const { return _mode == Mode::STA && WiFi.status() == WL_CONNECTED; }

  // Applied immediately from the web UI; persists via WifiCredStore and
  // attempts to connect. Falls back to staying in AP mode on failure.
  bool applyNewCredentials(const String &ssid, const String &password);

  void forceApMode();

  String statusSummary() const;
  time_t lastNtpSyncEpoch() const { return _lastNtpSyncEpoch; }
  bool ntpEverSynced() const { return _lastNtpSyncEpoch != 0; }

  // Set by main.cpp right after a successful NTP sync so the status page can
  // display it; NetworkManager itself does not perform NTP sync timing here
  // (see main.cpp) to keep RTC/NTP logic in one place.
  void noteNtpSync(time_t utcEpoch) { _lastNtpSyncEpoch = utcEpoch; }

private:
  WifiCredStore *_creds = nullptr;
  Mode _mode = Mode::AP;
  DNSServer _dnsServer;
  time_t _lastNtpSyncEpoch = 0;

  // Reset button debounce/hold detection
  uint32_t _buttonDownSince = 0;
  bool _buttonHoldHandled = false;

  // Background reconnect
  uint32_t _lastReconnectAttempt = 0;

  void startAp();
  void startSta();
  void handleResetButton();
  void handleReconnect();
};
