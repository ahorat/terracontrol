#pragma once

#include <RTClib.h>

// Wraps the DS3231 RTC. By convention the RTC always holds Europe/Zurich
// LOCAL wall-clock time (not UTC) - manual sets are written through as-is,
// NTP sync converts UTC -> local before writing. This keeps schedule
// evaluation and sunrise/sunset comparisons free of timezone math.
class RtcClock {
public:
  bool begin();

  // False if the DS3231 lost power (OSF flag) or holds an implausible date.
  // Callers must treat this as failsafe: all relays off.
  bool isTimeValid();

  DateTime now();

  // Manual set from the web UI: local wall-clock time, written straight to RTC.
  void setLocal(const DateTime &localTime);

  // Called after a successful NTP sync with the UTC epoch. Converts to
  // Europe/Zurich local (DST-aware) before writing to the RTC.
  void setFromUtcEpoch(time_t utcEpoch);

  // DST decision for a given LOCAL calendar date (used to pick the correct
  // UTC offset for sunrise/sunset calculation on that date). Approximates
  // the EU rule using local wall-clock fields; the only inaccuracy is the
  // repeated hour on the autumn transition day, which is inconsequential
  // here since it only affects which of two ~1h-apart sun times is used.
  static bool isDstForLocalDate(uint16_t year, uint8_t month, uint8_t day, uint8_t hour);

  // Current local UTC offset in seconds (3600 = CET, 7200 = CEST) for the
  // given local date.
  static long localUtcOffsetSeconds(const DateTime &localDate);

  // Converts a true UTC instant to Europe/Zurich local time (DST-aware,
  // using the correct UTC-instant EU rule). Used for setFromUtcEpoch() and
  // for displaying any other UTC timestamp (e.g. last-NTP-sync) as local.
  static DateTime utcToLocal(const DateTime &utc);

private:
  RTC_DS3231 _rtc;
  bool _present = false;

  // Correct EU DST rule evaluated on a true UTC instant (last Sunday of
  // March/October, 01:00 UTC), used only for the NTP UTC->local conversion.
  static bool isDstForUtcInstant(const DateTime &utc);
  static DateTime lastSundayOfMonth(uint16_t year, uint8_t month, uint8_t hour);
};
