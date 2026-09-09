#include <Arduino.h>
#include <Wire.h>
#include <time.h>
#include <esp_sntp.h>

#include "Config.h"
#include "ChannelConfig.h"
#include "EepromStore.h"
#include "WifiCredStore.h"
#include "RtcClock.h"
#include "RelayChannel.h"
#include "WifiController.h"
#include "WebPortal.h"

static RtcClock rtcClock;
static EepromStore eepromStore;
static WifiCredStore wifiCreds;
static WifiController netManager;
static WebPortal webPortal;
static RelayChannel channels[CHANNEL_COUNT];

// Called by the ESP32 SNTP client on every successful sync (boot + every
// NTP_RESYNC_INTERVAL_MS thereafter, see setup()). Keeps the DS3231 aligned
// with real time whenever a network is available.
static void onNtpSync(struct timeval *tv) {
  rtcClock.setFromUtcEpoch(tv->tv_sec);
  netManager.noteNtpSync(tv->tv_sec);
  Serial.println("[ntp] sync received, RTC updated");
}

void setup() {
  Serial.begin(115200);
  // ESP32-C6 has no external USB-UART bridge - the native USB port
  // re-enumerates on every reset/power-cycle, so a monitor reattaching after
  // that would otherwise miss these early boot lines entirely. This pause
  // gives it time to reconnect first. Harmless in normal (unattended)
  // operation - it only delays the very first relay evaluation by a few
  // seconds, not anything time-critical.
  delay(8000);
  Serial.println("\n[boot] WiFi 4-Kanal Relaiscontroller");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  eepromStore.begin();
  ChannelConfig configs[CHANNEL_COUNT];
  eepromStore.loadAll(configs);

  for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
    channels[i].begin(RELAY_PINS[i]);
    channels[i].setConfig(configs[i]);
  }

  if (!rtcClock.begin()) {
    Serial.println("[rtc] DS3231 not found on I2C bus - relays stay in failsafe (off)");
  } else if (!rtcClock.isTimeValid()) {
    Serial.println("[rtc] time invalid (power loss / implausible) - relays stay off until NTP or manual set");
  }

  wifiCreds.begin();
  netManager.begin(&wifiCreds);

  // NTP: sync at boot, then every 12h (see Config.h). We request raw UTC
  // (offsets 0,0) and do our own Europe/Zurich DST conversion in RtcClock,
  // since the RTC itself always stores local wall-clock time.
  sntp_set_time_sync_notification_cb(onNtpSync);
  sntp_set_sync_interval(NTP_RESYNC_INTERVAL_MS);
  configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);

  webPortal.begin(channels, &rtcClock, &eepromStore, &netManager);

  Serial.println("[boot] ready");
}

void loop() {
  netManager.loop();

  static uint32_t lastTick = 0;
  uint32_t nowMs = millis();
  if (nowMs - lastTick < SCHEDULER_TICK_MS) {
    return;
  }
  lastTick = nowMs;

  if (rtcClock.isTimeValid()) {
    DateTime now = rtcClock.now();
    long sunOffset = RtcClock::localUtcOffsetSeconds(now);
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
      channels[i].evaluate(now, sunOffset);
    }
  } else {
    // Failsafe: invalid RTC time (power loss / never set) - all relays off,
    // independent of configured schedule or WiFi state.
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
      channels[i].forceOff();
    }
  }
}
