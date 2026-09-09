#pragma once

#include <Arduino.h>

// Persists the target WiFi credentials in ESP32 NVS (Preferences), separate
// from the RTC's AT24C32 EEPROM which only holds channel configuration.
class WifiCredStore {
public:
  bool begin();
  bool hasCredentials() const;
  String ssid() const { return _ssid; }
  String password() const { return _password; }
  void save(const String &ssid, const String &password);
  void clear();

  // User-requested "WiFi off" state (see WifiController). Persisted so it
  // survives reboots/power loss - only the physical reset button clears it.
  bool wifiDisabled() const { return _wifiDisabled; }
  void setWifiDisabled(bool disabled);

private:
  String _ssid;
  String _password;
  bool _wifiDisabled = false;
};
