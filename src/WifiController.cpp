#include "WifiController.h"
#include "Config.h"
#include <ESPmDNS.h>
#include <esp32-hal-rgb-led.h>

static const uint8_t DNS_PORT = 53;

void WifiController::begin(WifiCredStore *credStore) {
  _creds = credStore;
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  if (_creds->hasCredentials()) {
    startSta();
  } else {
    startAp();
  }
}

void WifiController::startAp() {
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_LOCAL_IP, AP_GATEWAY, AP_SUBNET);
  WiFi.softAP(AP_SSID, strlen(AP_PASSWORD) ? AP_PASSWORD : nullptr);
  _dnsServer.start(DNS_PORT, "*", AP_LOCAL_IP);
  _mode = Mode::AP;
  _mdnsStarted = false;
  restartMdns();
  Serial.println("[net] AP mode: " + String(AP_SSID) + " @ " + AP_LOCAL_IP.toString());
}

void WifiController::startSta() {
  // Non-blocking: WiFi.begin() kicks off the connection asynchronously.
  // Success/failure is picked up via WiFi.status() from loop()/handleReconnect()
  // and the status JSON endpoint, so this never stalls relay scheduling.
  _dnsServer.stop();
  WiFi.mode(WIFI_STA);
  WiFi.begin(_creds->ssid().c_str(), _creds->password().c_str());
  _mode = Mode::STA;
  _mdnsStarted = false; // no IP yet; loop() starts mDNS once connected
  _lastReconnectAttempt = millis();
  Serial.println("[net] STA mode, connecting to " + _creds->ssid());
}

void WifiController::restartMdns() {
  MDNS.end();
  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[net] mDNS: http://" + String(MDNS_HOSTNAME) + ".local/");
  }
  _mdnsStarted = true;
}

bool WifiController::applyNewCredentials(const String &ssid, const String &password) {
  if (ssid.length() == 0) return false;
  _creds->save(ssid, password);
  startSta();
  return true;
}

void WifiController::forceApMode() {
  startAp();
}

void WifiController::handleResetButton() {
  bool pressed = digitalRead(RESET_BUTTON_PIN) == LOW;

  if (pressed) {
    if (_buttonDownSince == 0) {
      _buttonDownSince = millis();
      _buttonHoldHandled = false;
    } else if (!_buttonHoldHandled && millis() - _buttonDownSince >= RESET_BUTTON_HOLD_MS) {
      _buttonHoldHandled = true;
      Serial.println("[net] reset button held, forcing AP mode");
      forceApMode();
    }
  } else {
    _buttonDownSince = 0;
    _buttonHoldHandled = false;
  }
}

void WifiController::handleReconnect() {
  if (_mode != Mode::STA) return;
  if (WiFi.status() == WL_CONNECTED) return;

  if (millis() - _lastReconnectAttempt >= WIFI_RECONNECT_INTERVAL_MS) {
    _lastReconnectAttempt = millis();
    Serial.println("[net] attempting background reconnect");
    WiFi.disconnect();
    WiFi.begin(_creds->ssid().c_str(), _creds->password().c_str());
  }
}

void WifiController::loop() {
  handleResetButton();
  handleReconnect();
  if (_mode == Mode::AP) {
    _dnsServer.processNextRequest();
  } else if (_mode == Mode::STA) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!_mdnsStarted) restartMdns();
    } else {
      _mdnsStarted = false; // re-announce once reconnected
    }
  }
  updateStatusLed();
}

void WifiController::updateStatusLed() {
  uint32_t now = millis();
  if (now - _lastLedToggle >= RGB_LED_BLINK_INTERVAL_MS) {
    _lastLedToggle = now;
    _ledOn = !_ledOn;
  }

  uint8_t r = 0, g = 0, b = 0;
  if (_ledOn) {
    if (!_creds->hasCredentials()) {
      r = RGB_LED_BRIGHTNESS; // red: no WiFi configured
    } else if (!staConnected()) {
      b = RGB_LED_BRIGHTNESS; // blue: configured, not (yet) connected
    } else {
      g = RGB_LED_BRIGHTNESS; // green: configured and connected
    }
  }

  if (r != _lastLedR || g != _lastLedG || b != _lastLedB) {
    rgbLedWrite(RGB_LED_PIN, r, g, b);
    _lastLedR = r;
    _lastLedG = g;
    _lastLedB = b;
  }
}

String WifiController::statusSummary() const {
  if (_mode == Mode::AP) {
    return "Access-Point-Modus (" + String(AP_SSID) + ")";
  }
  if (WiFi.status() == WL_CONNECTED) {
    return "Verbunden mit " + WiFi.SSID() + " (" + WiFi.localIP().toString() + ")";
  }
  return "Getrennt, Wiederverbindung wird versucht";
}
