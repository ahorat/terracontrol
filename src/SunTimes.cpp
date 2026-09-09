#include "SunTimes.h"
#include "Config.h"
#include <SolarCalculator.h>
#include <math.h>

namespace SunTimes {

SunEvent calculate(uint16_t year, uint8_t month, uint8_t day, long utcOffsetSeconds) {
  SunEvent ev;

  double transit, sunriseUtcHours, sunsetUtcHours;
  // SolarCalculator's calcSunriseSunset() takes/returns UTC; we convert the
  // (fractional-hour) UTC results to local afterwards using the caller's
  // DST-aware offset for this date.
  calcSunriseSunset(year, month, day, LOCATION_LAT, LOCATION_LON, transit, sunriseUtcHours, sunsetUtcHours);

  if (isnan(sunriseUtcHours) || isnan(sunsetUtcHours)) {
    ev.valid = false;
    return ev;
  }

  DateTime midnightUtc(year, month, day, 0, 0, 0);
  int32_t sunriseSec = (int32_t)lround(sunriseUtcHours * 3600.0);
  int32_t sunsetSec = (int32_t)lround(sunsetUtcHours * 3600.0);

  ev.sunrise = midnightUtc + TimeSpan(sunriseSec) + TimeSpan((int32_t)utcOffsetSeconds);
  ev.sunset = midnightUtc + TimeSpan(sunsetSec) + TimeSpan((int32_t)utcOffsetSeconds);
  ev.valid = true;
  return ev;
}

} // namespace SunTimes
