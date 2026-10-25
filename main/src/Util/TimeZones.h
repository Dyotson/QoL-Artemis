#ifndef ARTEMIS_FIRMWARE_TIMEZONES_H
#define ARTEMIS_FIRMWARE_TIMEZONES_H

#include <cstdint>
#include <ctime>

/**
 * Self-contained POSIX TZ rule evaluation (e.g. "CET-1CEST,M3.5.0,M10.5.0/3").
 * newlib's localtime() would need setenv("TZ") which is process-global and would
 * disturb the rest of the firmware (it keeps local time and relies on mktime/gmtime).
 */
namespace TimeZones {

struct City {
	const char* name;
	const char* tz; // POSIX TZ rule
};

extern const City Cities[];
extern const uint8_t CityCount;

/** UTC offset in minutes (east positive) of a POSIX TZ rule at the given UTC instant */
int16_t offsetAt(const char* tz, time_t utc);

/** Local broken-down time of a city at the given UTC instant */
tm localTime(const City& city, time_t utc, int16_t* offsetOut = nullptr);

}

#endif //ARTEMIS_FIRMWARE_TIMEZONES_H
