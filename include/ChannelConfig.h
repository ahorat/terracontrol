#pragma once

#include <Arduino.h>

// Operating mode of a single relay channel.
enum ChannelMode : uint8_t {
  MODE_DISABLED = 0,   // relay stays off, no schedule evaluated
  MODE_INTERVAL = 1,   // repeats every N minutes for M seconds
  MODE_FIXED_TIME = 2, // fixed on/off time of day
  MODE_SUN = 3         // sunrise/sunset based, with per-edge offset
};

// Configuration for one channel. Kept as a flat POD struct so it can be
// serialized to/from the AT24C32 EEPROM with a fixed byte layout
// (see EepromStore.cpp) and to/from JSON for the web UI (see WebPortal.cpp).
struct ChannelConfig {
  ChannelMode mode = MODE_DISABLED;

  // Weekday restriction, bit i = 1<<wday with wday per struct tm (0=Sunday..6=Saturday).
  // Default: every day enabled.
  uint8_t weekdayMask = 0x7F;

  // Optional time-of-day window restricting MODE_INTERVAL. Half-open
  // [start, end) range in minutes since midnight (end = 1440 means midnight,
  // i.e. "until end of day"); start > end wraps past midnight.
  bool windowEnabled = false;
  uint16_t windowStartMin = 0;
  uint16_t windowEndMin = 1440;

  // MODE_INTERVAL parameters
  uint16_t intervalMinutes = 30;
  uint16_t onSeconds = 10;

  // MODE_FIXED_TIME parameters (minutes since midnight)
  uint16_t fixedOnMin = 480;   // 08:00
  uint16_t fixedOffMin = 1200; // 20:00

  // MODE_SUN parameters
  int16_t sunriseOffsetMin = 0;
  int16_t sunsetOffsetMin = 0;
  // true: channel turns ON at sunrise+offset and OFF at sunset+offset.
  // false: channel turns ON at sunset+offset and OFF at sunrise+offset
  // (typical use case: garden/path lighting).
  bool onAtSunrise = false;

  bool isDayAllowed(uint8_t wday) const {
    return (weekdayMask & (1 << wday)) != 0;
  }
};

// Fixed on-wire size of one serialized ChannelConfig, see EepromStore.cpp.
static const size_t CHANNEL_CONFIG_BYTES = 20;
