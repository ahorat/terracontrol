#pragma once

#include <RTClib.h>
#include "ChannelConfig.h"
#include "SunTimes.h"

// Runtime state + schedule evaluation for a single relay channel. All 4
// hardware channels share this one implementation (see main.cpp), only the
// GPIO pin and ChannelConfig differ.
class RelayChannel {
public:
  void begin(uint8_t pin);

  void setConfig(const ChannelConfig &cfg) { _cfg = cfg; }
  const ChannelConfig &config() const { return _cfg; }

  // Re-evaluates the schedule/override for `nowLocal` and drives the relay
  // GPIO accordingly. Call once per scheduler tick for every channel.
  // `sunOffset` is the DST-aware UTC offset (seconds) for `nowLocal`'s date.
  void evaluate(const DateTime &nowLocal, long sunUtcOffsetSeconds);

  // Failsafe: force the relay off immediately (invalid RTC time etc.),
  // without touching the configured schedule or any active override.
  void forceOff();

  bool isOn() const { return _relayOn; }

  // Manual override, see Aufgabenbeschreibung 3.1 Punkt 4.
  // durationSeconds == 0 means "until manually cleared".
  void setOverride(bool state, uint32_t durationSeconds, const DateTime &nowLocal);
  void clearOverride();
  bool overrideActive() const { return _overrideActive; }
  bool overrideIndefinite() const { return _overrideActive && _overrideIndefinite; }
  const DateTime &overrideEndsAt() const { return _overrideEnd; }

  // Human-readable mode name, for the dashboard.
  const char *modeName() const;

  // Best-effort next schedule transition (ignores override), or an invalid
  // DateTime (year 2000) if the mode has no future transition (disabled).
  DateTime nextScheduledTransition(const DateTime &nowLocal, long sunUtcOffsetSeconds) const;

private:
  uint8_t _pin = 255;
  ChannelConfig _cfg;
  bool _relayOn = false;

  bool _overrideActive = false;
  bool _overrideIndefinite = false;
  bool _overrideState = false;
  DateTime _overrideEnd = DateTime((uint32_t)0);

  void applyRelay(bool on);

  // Pure "what should the schedule say" evaluation, ignoring override.
  bool scheduledState(const DateTime &nowLocal, long sunUtcOffsetSeconds) const;

  bool intervalActive(const DateTime &nowLocal) const;
  bool fixedTimeActive(const DateTime &nowLocal) const;
  bool sunActive(const DateTime &nowLocal, long sunUtcOffsetSeconds) const;

  DateTime nextIntervalTransition(const DateTime &nowLocal) const;
  DateTime nextFixedTimeTransition(const DateTime &nowLocal) const;
  DateTime nextSunTransition(const DateTime &nowLocal, long sunUtcOffsetSeconds) const;

  bool weekdayAllowed(const DateTime &t) const { return _cfg.isDayAllowed(t.dayOfTheWeek()); }
};
