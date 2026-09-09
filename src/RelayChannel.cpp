#include "RelayChannel.h"
#include "Config.h"
#include <vector>
#include <algorithm>

// Half-open [start, end) range over minutes-of-day, wrapping past midnight
// when start > end (e.g. 22:00 - 06:00).
static bool inMinuteWindow(uint16_t minuteOfDay, uint16_t start, uint16_t end) {
  if (start == end) return false;
  if (start < end) {
    return minuteOfDay >= start && minuteOfDay < end;
  }
  return minuteOfDay >= start || minuteOfDay < end;
}

void RelayChannel::begin(uint8_t pin) {
  _pin = pin;
  // pinMode() must come first: on current arduino-esp32, digitalWrite() on a
  // pin not yet claimed as GPIO is rejected outright (silently does nothing),
  // which left the pin on pinMode(OUTPUT)'s default LOW level - energizing
  // an active-LOW relay immediately at boot despite _relayOn saying false.
  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, RELAY_ACTIVE_LOW ? HIGH : LOW);
  _relayOn = false; // matches the level just written; failsafe default
  Serial.printf("[relay] pin %u init -> OFF\n", _pin);
}

void RelayChannel::applyRelay(bool on) {
  _relayOn = on;
  bool pinHigh = RELAY_ACTIVE_LOW ? !on : on;
  digitalWrite(_pin, pinHigh ? HIGH : LOW);
  Serial.printf("[relay] pin %u (mode=%d) -> %s\n", _pin, (int)_cfg.mode, on ? "ON" : "OFF");
}

void RelayChannel::forceOff() {
  applyRelay(false);
}

// ---------------------------------------------------------------------------
// Override
// ---------------------------------------------------------------------------

void RelayChannel::setOverride(bool state, uint32_t durationSeconds, const DateTime &nowLocal) {
  _overrideActive = true;
  _overrideState = state;
  _overrideIndefinite = (durationSeconds == 0);
  _overrideEnd = _overrideIndefinite ? nowLocal : (nowLocal + TimeSpan((int32_t)durationSeconds));
}

void RelayChannel::clearOverride() {
  _overrideActive = false;
  _overrideIndefinite = false;
}

// ---------------------------------------------------------------------------
// Mode-specific "is it active right now" checks
// ---------------------------------------------------------------------------

bool RelayChannel::intervalActive(const DateTime &nowLocal) const {
  if (!weekdayAllowed(nowLocal)) return false;
  uint16_t minuteOfDay = nowLocal.hour() * 60 + nowLocal.minute();
  if (_cfg.windowEnabled && !inMinuteWindow(minuteOfDay, _cfg.windowStartMin, _cfg.windowEndMin)) {
    return false;
  }
  if (_cfg.intervalMinutes == 0) return false;
  uint32_t periodSec = (uint32_t)_cfg.intervalMinutes * 60UL;
  uint32_t secondsOfDay = (uint32_t)minuteOfDay * 60UL + nowLocal.second();
  uint32_t posInCycle = secondsOfDay % periodSec;
  return posInCycle < _cfg.onSeconds;
}

bool RelayChannel::fixedTimeActive(const DateTime &nowLocal) const {
  if (!weekdayAllowed(nowLocal)) return false;
  uint16_t minuteOfDay = nowLocal.hour() * 60 + nowLocal.minute();
  return inMinuteWindow(minuteOfDay, _cfg.fixedOnMin, _cfg.fixedOffMin);
}

bool RelayChannel::sunActive(const DateTime &nowLocal, long sunUtcOffsetSeconds) const {
  if (!weekdayAllowed(nowLocal)) return false;

  SunTimes::SunEvent ev = SunTimes::calculate(nowLocal.year(), nowLocal.month(), nowLocal.day(), sunUtcOffsetSeconds);
  if (!ev.valid) return false;

  DateTime onTime = (_cfg.onAtSunrise ? ev.sunrise : ev.sunset) + TimeSpan((int32_t)(_cfg.onAtSunrise ? _cfg.sunriseOffsetMin : _cfg.sunsetOffsetMin) * 60);
  DateTime offTime = (_cfg.onAtSunrise ? ev.sunset : ev.sunrise) + TimeSpan((int32_t)(_cfg.onAtSunrise ? _cfg.sunsetOffsetMin : _cfg.sunriseOffsetMin) * 60);

  uint16_t onMin = onTime.hour() * 60 + onTime.minute();
  uint16_t offMin = offTime.hour() * 60 + offTime.minute();
  uint16_t minuteOfDay = nowLocal.hour() * 60 + nowLocal.minute();
  return inMinuteWindow(minuteOfDay, onMin, offMin);
}

bool RelayChannel::scheduledState(const DateTime &nowLocal, long sunUtcOffsetSeconds) const {
  switch (_cfg.mode) {
    case MODE_INTERVAL: return intervalActive(nowLocal);
    case MODE_FIXED_TIME: return fixedTimeActive(nowLocal);
    case MODE_SUN: return sunActive(nowLocal, sunUtcOffsetSeconds);
    case MODE_DISABLED:
    default: return false;
  }
}

// ---------------------------------------------------------------------------
// Main tick
// ---------------------------------------------------------------------------

void RelayChannel::evaluate(const DateTime &nowLocal, long sunUtcOffsetSeconds) {
  if (_overrideActive && !_overrideIndefinite && nowLocal.unixtime() >= _overrideEnd.unixtime()) {
    clearOverride();
  }

  bool desired;
  if (_overrideActive) {
    desired = _overrideState;
  } else {
    desired = scheduledState(nowLocal, sunUtcOffsetSeconds);
  }

  if (desired != _relayOn) {
    applyRelay(desired);
  }
}

const char *RelayChannel::modeName() const {
  switch (_cfg.mode) {
    case MODE_INTERVAL: return "Intervall";
    case MODE_FIXED_TIME: return "Feste Tageszeit";
    case MODE_SUN: return "Sonnenauf-/untergang";
    case MODE_DISABLED:
    default: return "Deaktiviert";
  }
}

// ---------------------------------------------------------------------------
// Next transition (best-effort, for the dashboard)
// ---------------------------------------------------------------------------

DateTime RelayChannel::nextIntervalTransition(const DateTime &nowLocal) const {
  if (_cfg.intervalMinutes == 0) return DateTime((uint32_t)0);
  uint32_t periodSec = (uint32_t)_cfg.intervalMinutes * 60UL;

  // State right now, used as the baseline to detect the first real flip.
  bool prevState = weekdayAllowed(nowLocal) ? intervalActive(nowLocal) : false;

  for (int dayOffset = 0; dayOffset < 8; dayOffset++) {
    DateTime day = nowLocal + TimeSpan(dayOffset, 0, 0, 0);
    if (!weekdayAllowed(day)) {
      prevState = false;
      continue;
    }
    DateTime midnight(day.year(), day.month(), day.day(), 0, 0, 0);

    uint32_t startSearch = (dayOffset == 0)
        ? (nowLocal.hour() * 3600UL + nowLocal.minute() * 60UL + nowLocal.second() + 1)
        : 0;

    // Candidate transition instants: every on/off edge of the interval cycle,
    // plus the window's own edges (entering/leaving the window can itself
    // force a transition regardless of cycle phase). Not every candidate is
    // a genuine flip (e.g. a cycle edge that falls outside the window), so
    // each one is re-checked against intervalActive() below.
    std::vector<uint32_t> candidates;
    for (uint32_t k = 0; k * periodSec < 86400UL; k++) {
      candidates.push_back(k * periodSec);
      candidates.push_back(k * periodSec + _cfg.onSeconds);
    }
    if (_cfg.windowEnabled) {
      candidates.push_back((uint32_t)_cfg.windowStartMin * 60UL);
      if (_cfg.windowEndMin < 1440) candidates.push_back((uint32_t)_cfg.windowEndMin * 60UL);
    }
    std::sort(candidates.begin(), candidates.end());

    for (uint32_t sec : candidates) {
      if (sec < startSearch || sec >= 86400UL) continue;
      DateTime candTime = midnight + TimeSpan((int32_t)sec);
      bool state = intervalActive(candTime);
      if (state != prevState) return candTime;
      prevState = state;
    }
  }
  return DateTime((uint32_t)0);
}

DateTime RelayChannel::nextFixedTimeTransition(const DateTime &nowLocal) const {
  for (int dayOffset = 0; dayOffset < 8; dayOffset++) {
    DateTime day = nowLocal + TimeSpan(dayOffset, 0, 0, 0);
    if (!weekdayAllowed(day)) continue;

    DateTime midnight(day.year(), day.month(), day.day(), 0, 0, 0);
    DateTime candidates[2] = {
      midnight + TimeSpan((int32_t)_cfg.fixedOnMin * 60),
      midnight + TimeSpan((int32_t)_cfg.fixedOffMin * 60)
    };
    for (DateTime c : candidates) {
      if (c.unixtime() > nowLocal.unixtime()) return c;
    }
  }
  return DateTime((uint32_t)0);
}

DateTime RelayChannel::nextSunTransition(const DateTime &nowLocal, long sunUtcOffsetSeconds) const {
  for (int dayOffset = 0; dayOffset < 8; dayOffset++) {
    DateTime day = nowLocal + TimeSpan(dayOffset, 0, 0, 0);
    if (!weekdayAllowed(day)) continue;

    SunTimes::SunEvent ev = SunTimes::calculate(day.year(), day.month(), day.day(), sunUtcOffsetSeconds);
    if (!ev.valid) continue;

    DateTime onTime = (_cfg.onAtSunrise ? ev.sunrise : ev.sunset) + TimeSpan((int32_t)(_cfg.onAtSunrise ? _cfg.sunriseOffsetMin : _cfg.sunsetOffsetMin) * 60);
    DateTime offTime = (_cfg.onAtSunrise ? ev.sunset : ev.sunrise) + TimeSpan((int32_t)(_cfg.onAtSunrise ? _cfg.sunsetOffsetMin : _cfg.sunriseOffsetMin) * 60);

    DateTime candidates[2] = {onTime, offTime};
    for (DateTime c : candidates) {
      if (c.unixtime() > nowLocal.unixtime()) return c;
    }
  }
  return DateTime((uint32_t)0);
}

DateTime RelayChannel::nextScheduledTransition(const DateTime &nowLocal, long sunUtcOffsetSeconds) const {
  switch (_cfg.mode) {
    case MODE_INTERVAL: return nextIntervalTransition(nowLocal);
    case MODE_FIXED_TIME: return nextFixedTimeTransition(nowLocal);
    case MODE_SUN: return nextSunTransition(nowLocal, sunUtcOffsetSeconds);
    case MODE_DISABLED:
    default: return DateTime((uint32_t)0);
  }
}
