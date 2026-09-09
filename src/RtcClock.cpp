#include "RtcClock.h"

// RTClib's DateTime::dayOfTheWeek() returns 0=Sunday .. 6=Saturday (verified
// against RTClib's date2days()-based implementation), matching struct tm's
// tm_wday and the ChannelConfig::weekdayMask bit convention used throughout.

static const uint8_t DAYS_IN_MONTH[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static bool isLeapYear(uint16_t year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

bool RtcClock::begin() {
  _present = _rtc.begin(&Wire);
  return _present;
}

bool RtcClock::isTimeValid() {
  if (!_present) return false;
  if (_rtc.lostPower()) return false;
  DateTime n = _rtc.now();
  if (!n.isValid()) return false;
  if (n.year() < 2024 || n.year() > 2099) return false;
  return true;
}

DateTime RtcClock::now() {
  return _rtc.now();
}

void RtcClock::setLocal(const DateTime &localTime) {
  if (!_present) return;
  _rtc.adjust(localTime); // also clears the DS3231 OSF (lost-power) flag
}

DateTime RtcClock::lastSundayOfMonth(uint16_t year, uint8_t month, uint8_t hour) {
  uint8_t lastDay = DAYS_IN_MONTH[month - 1];
  if (month == 2 && isLeapYear(year)) lastDay = 29;
  DateTime d(year, month, lastDay, hour, 0, 0);
  uint8_t wday = d.dayOfTheWeek(); // 0=Sunday
  return d - TimeSpan(wday, 0, 0, 0);
}

bool RtcClock::isDstForUtcInstant(const DateTime &utc) {
  DateTime start = lastSundayOfMonth(utc.year(), 3, 1);  // 01:00 UTC
  DateTime end = lastSundayOfMonth(utc.year(), 10, 1);   // 01:00 UTC
  return utc.unixtime() >= start.unixtime() && utc.unixtime() < end.unixtime();
}

bool RtcClock::isDstForLocalDate(uint16_t year, uint8_t month, uint8_t day, uint8_t hour) {
  DateTime cur(year, month, day, hour, 0, 0);
  DateTime start = lastSundayOfMonth(year, 3, 2);  // clocks jump 02:00->03:00 local
  DateTime end = lastSundayOfMonth(year, 10, 3);   // clocks fall back 03:00->02:00 local
  return cur.unixtime() >= start.unixtime() && cur.unixtime() < end.unixtime();
}

long RtcClock::localUtcOffsetSeconds(const DateTime &localDate) {
  bool dst = isDstForLocalDate(localDate.year(), localDate.month(), localDate.day(), localDate.hour());
  return dst ? 7200L : 3600L;
}

void RtcClock::setFromUtcEpoch(time_t utcEpoch) {
  if (!_present) return;
  DateTime utc((uint32_t)utcEpoch);
  bool dst = isDstForUtcInstant(utc);
  long offset = dst ? 7200L : 3600L;
  DateTime local = utc + TimeSpan((int32_t)offset);
  _rtc.adjust(local);
}
