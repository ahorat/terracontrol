#include "WifiCredStore.h"
#include <Preferences.h>

static const char *NVS_NAMESPACE = "wifi";

bool WifiCredStore::begin() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) { // read-only
    return false;
  }
  _ssid = prefs.getString("ssid", "");
  _password = prefs.getString("pass", "");
  _wifiDisabled = prefs.getBool("disabled", false);
  prefs.end();
  return true;
}

bool WifiCredStore::hasCredentials() const {
  return _ssid.length() > 0;
}

void WifiCredStore::save(const String &ssid, const String &password) {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", password);
  prefs.end();
  _ssid = ssid;
  _password = password;
}

void WifiCredStore::setWifiDisabled(bool disabled) {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putBool("disabled", disabled);
  prefs.end();
  _wifiDisabled = disabled;
}

void WifiCredStore::clear() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.clear();
  prefs.end();
  _ssid = "";
  _password = "";
  _wifiDisabled = false;
}
