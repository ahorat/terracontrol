#pragma once

#include <RTClib.h>

// Sunrise/sunset calculation for the fixed reference location (Bern, CH),
// DST-aware via the caller-supplied UTC offset for the requested date.
namespace SunTimes {

struct SunEvent {
  DateTime sunrise; // local time
  DateTime sunset;  // local time
  bool valid = false;
};

// Computes sunrise/sunset for the given local calendar date (year/month/day).
// utcOffsetSeconds must be the offset that applies on that date (3600 for
// CET, 7200 for CEST) as determined by RtcClock's DST rule.
SunEvent calculate(uint16_t year, uint8_t month, uint8_t day, long utcOffsetSeconds);

} // namespace SunTimes
