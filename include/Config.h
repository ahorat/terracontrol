#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Hardware pin assignment (freely reassignable, see Aufgabenbeschreibung 5.)
// Chosen to avoid ESP32 strapping pins (0,2,4,5,12,15), UART0 (1,3) and the
// input-only pins (34-39), so relays never glitch during boot.
// ---------------------------------------------------------------------------
static const uint8_t RELAY_PINS[4] = {13, 14, 27, 26};
// Most cheap 5V relay boards (SRD-05VDC-SL-C based) are opto-isolated and
// active-LOW (GPIO LOW energizes the relay). Flip to false for active-HIGH
// boards.
static const bool RELAY_ACTIVE_LOW = true;
static const uint8_t RESET_BUTTON_PIN = 33;   // pull-up, active LOW
static const uint32_t RESET_BUTTON_HOLD_MS = 3000; // > 3s to trigger AP mode

// I2C (DS3231 + AT24C32 share the bus, default ESP32 Wire pins)
static const uint8_t I2C_SDA_PIN = 21;
static const uint8_t I2C_SCL_PIN = 22;
static const uint8_t AT24C32_I2C_ADDR = 0x57;

// ---------------------------------------------------------------------------
// Location: Bern, Switzerland (fixed, no GPS/manual entry per spec)
// ---------------------------------------------------------------------------
static const double LOCATION_LAT = 46.9480;
static const double LOCATION_LON = 7.4474;

// ---------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------
static const char *AP_SSID = "RelayController-Setup";
static const char *AP_PASSWORD = ""; // open AP for easy first setup
static const IPAddress AP_LOCAL_IP(192, 168, 4, 1);
static const IPAddress AP_GATEWAY(192, 168, 4, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);

static const uint32_t WIFI_RECONNECT_INTERVAL_MS = 5UL * 60UL * 1000UL; // ~5 min

// ---------------------------------------------------------------------------
// NTP
// ---------------------------------------------------------------------------
static const char *NTP_SERVER_1 = "pool.ntp.org";
static const char *NTP_SERVER_2 = "ch.pool.ntp.org";
static const uint32_t NTP_RESYNC_INTERVAL_MS = 12UL * 60UL * 60UL * 1000UL; // 12h

// ---------------------------------------------------------------------------
// Misc
// ---------------------------------------------------------------------------
static const uint8_t CHANNEL_COUNT = 4;
static const uint32_t SCHEDULER_TICK_MS = 1000; // relay evaluation interval
