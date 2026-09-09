#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Hardware pin assignment (freely reassignable, see Aufgabenbeschreibung 5.)
// Board: Waveshare ESP32-C6-Zero. Unlike the classic ESP32, GPIO0-3 on the
// C6 carry no strapping/UART/USB role at all, so they're plain safe outputs
// here. GPIO4 is technically a strapping pin (MTMS/JTAG source select) but
// the datasheet lists "any value" as acceptable for normal SPI-boot, so it's
// safe as a pulled-up button input. Avoided on purpose: GPIO8 (onboard
// WS2812 LED + boot-log routing), GPIO9 (onboard BOOT button, must read
// HIGH at reset), GPIO12/13 (native USB D-/D+, used by the board's USB-C
// port for flashing/serial).
// ---------------------------------------------------------------------------
static const uint8_t RELAY_PINS[4] = {0, 1, 2, 3};
// Most cheap 5V relay boards (SRD-05VDC-SL-C based) are opto-isolated and
// active-LOW (GPIO LOW energizes the relay). Flip to false for active-HIGH
// boards.
static const bool RELAY_ACTIVE_LOW = true;
static const uint8_t RESET_BUTTON_PIN = 4;   // pull-up, active LOW
static const uint32_t RESET_BUTTON_HOLD_MS = 3000; // > 3s to trigger AP mode

// I2C (DS3231 + AT24C32 share the bus)
static const uint8_t I2C_SDA_PIN = 21;
static const uint8_t I2C_SCL_PIN = 22;
static const uint8_t AT24C32_I2C_ADDR = 0x57;

// Onboard WS2812 RGB LED, used as a WiFi status indicator (see
// WifiController): red = not configured, blue = configured but not
// connected, green = configured and connected. Blinks at ~0.5Hz.
static const uint8_t RGB_LED_PIN = 8;
static const uint8_t RGB_LED_BRIGHTNESS = 40; // 0-255, kept low to avoid glare
static const uint32_t RGB_LED_BLINK_INTERVAL_MS = 1000; // toggle every 1s -> ~0.5Hz

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

// Reachable as http://<MDNS_HOSTNAME>.local/ once on the target WiFi, since
// the device's DHCP-assigned IP isn't known in advance.
static const char *MDNS_HOSTNAME = "terracontrol";

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
