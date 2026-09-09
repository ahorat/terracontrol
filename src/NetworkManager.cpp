#include "NetworkManager.h"
#include "Config.h"

static const uint8_t DNS_PORT = 53;

void NetworkManager::begin(WifiCredStore *credStore) {
  _creds = credStore;
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  if (_creds->hasCredentials()) {
    startSta();
  } else {
    startAp();
  }
}

void NetworkManager::startAp() {
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_LOCAL_IP, AP_GATEWAY, AP_SUBNET);
  WiFi.softAP(AP_SSID, strlen(AP_PASSWORD) ? AP_PASSWORD : nullptr);
  _dnsServer.start(DNS_PORT, "*", AP_LOCAL_IP);
  _mode = Mode::AP;
  Serial.println("[net] AP mode: " + String(AP_SSID) + " @ " + AP_LOCAL_IP.toString());
}

void NetworkManager::startSta() {
  // Non-blocking: WiFi.begin() kicks off the connection asynchronously.
  // Success/failure is picked up via WiFi.status() from loop()/handleReconnect()
  // and the status JSON endpoint, so this never stalls relay scheduling.
  _dnsServer.stop();
  WiFi.mode(WIFI_STA);
  WiFi.begin(_creds->ssid().c_str(), _creds->password().c_str());
  _mode = Mode::STA;
  _lastReconnectAttempt = millis();
  Serial.println("[net] STA mode, connecting to " + _creds->ssid());
}

bool NetworkManager::applyNewCredentials(const String &ssid, const String &password) {
  if (ssid.length() == 0) return false;
  _creds->save(ssid, password);
  startSta();
  return true;
}

void NetworkManager::forceApMode() {
  startAp();
}

void NetworkManager::handleResetButton() {
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

void NetworkManager::handleReconnect() {
  if (_mode != Mode::STA) return;
  if (WiFi.status() == WL_CONNECTED) return;

  if (millis() - _lastReconnectAttempt >= WIFI_RECONNECT_INTERVAL_MS) {
    _lastReconnectAttempt = millis();
    Serial.println("[net] attempting background reconnect");
    WiFi.disconnect();
    WiFi.begin(_creds->ssid().c_str(), _creds->password().c_str());
  }
}

void NetworkManager::loop() {
  handleResetButton();
  handleReconnect();
  if (_mode == Mode::AP) {
    _dnsServer.processNextRequest();
  }
}

String NetworkManager::statusSummary() const {
  if (_mode == Mode::AP) {
    return "Access-Point-Modus (" + String(AP_SSID) + ")";
  }
  if (WiFi.status() == WL_CONNECTED) {
    return "Verbunden mit " + WiFi.SSID() + " (" + WiFi.localIP().toString() + ")";
  }
  return "Getrennt, Wiederverbindung wird versucht";
}
