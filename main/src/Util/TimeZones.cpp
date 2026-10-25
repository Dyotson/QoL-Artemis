#include "TimeZones.h"
#include <cctype>
#include <cstdlib>

namespace TimeZones {

// Rules current as of 2026. Sorted west to east.
const City Cities[] = {
		{ "Honolulu", "HST10" },
		{ "Anchorage", "AKST9AKDT,M3.2.0,M11.1.0" },
		{ "Los Angeles", "PST8PDT,M3.2.0,M11.1.0" },
		{ "Denver", "MST7MDT,M3.2.0,M11.1.0" },
		{ "Mexico City", "CST6" },
		{ "Chicago", "CST6CDT,M3.2.0,M11.1.0" },
		{ "New York", "EST5EDT,M3.2.0,M11.1.0" },
		{ "Bogota", "<-05>5" },
		{ "Lima", "<-05>5" },
		{ "Caracas", "<-04>4" },
		{ "Santiago", "<-04>4<-03>,M9.1.6/24,M4.1.6/24" },
		{ "Buenos Aires", "<-03>3" },
		{ "Montevideo", "<-03>3" },
		{ "Sao Paulo", "<-03>3" },
		{ "Reykjavik", "GMT0" },
		{ "UTC", "UTC0" },
		{ "London", "GMT0BST,M3.5.0/1,M10.5.0" },
		{ "Lisbon", "WET0WEST,M3.5.0/1,M10.5.0" },
		{ "Madrid", "CET-1CEST,M3.5.0,M10.5.0/3" },
		{ "Paris", "CET-1CEST,M3.5.0,M10.5.0/3" },
		{ "Berlin", "CET-1CEST,M3.5.0,M10.5.0/3" },
		{ "Rome", "CET-1CEST,M3.5.0,M10.5.0/3" },
		{ "Athens", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
		{ "Cairo", "EET-2EEST,M4.5.5/0,M10.5.4/24" },
		{ "Istanbul", "<+03>-3" },
		{ "Moscow", "MSK-3" },
		{ "Dubai", "<+04>-4" },
		{ "Karachi", "PKT-5" },
		{ "Delhi", "IST-5:30" },
		{ "Dhaka", "<+06>-6" },
		{ "Bangkok", "<+07>-7" },
		{ "Singapore", "<+08>-8" },
		{ "Hong Kong", "HKT-8" },
		{ "Beijing", "CST-8" },
		{ "Seoul", "KST-9" },
		{ "Tokyo", "JST-9" },
		{ "Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3" },
		{ "Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3" },
};
const uint8_t CityCount = sizeof(Cities) / sizeof(Cities[0]);

static int64_t daysFromCivil(int y, unsigned m, unsigned d){
	y -= m <= 2;
	const int era = (y >= 0 ? y : y - 399) / 400;
	const unsigned yoe = (unsigned) (y - era * 400);
	const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return (int64_t) era * 146097 + (int64_t) doe - 719468;
}

static bool isLeap(int y){
	return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

static const char* skipName(const char* p){
	if(*p == '<'){
		while(*p && *p != '>') p++;
		return *p ? p + 1 : p;
	}
	while(*p && isalpha((unsigned char) *p)) p++;
	return p;
}

// Parses [+-]hh[:mm[:ss]] into seconds
static const char* parseTime(const char* p, int32_t& secs){
	int sign = 1;
	if(*p == '+') p++;
	else if(*p == '-'){ sign = -1; p++; }

	int32_t h = strtol(p, (char**) &p, 10), m = 0, s = 0;
	if(*p == ':'){ m = strtol(p + 1, (char**) &p, 10); }
	if(*p == ':'){ s = strtol(p + 1, (char**) &p, 10); }
	secs = sign * (h * 3600 + m * 60 + s);
	return p;
}

struct Rule {
	int month, week, day;
	int32_t time = 7200; // default 02:00 local
};

static const char* parseRule(const char* p, Rule& r){
	if(*p != 'M') return nullptr; // only the "Mm.w.d" form is used in the table
	r.month = strtol(p + 1, (char**) &p, 10);
	if(*p != '.') return nullptr;
	r.week = strtol(p + 1, (char**) &p, 10);
	if(*p != '.') return nullptr;
	r.day = strtol(p + 1, (char**) &p, 10);
	if(*p == '/') p = parseTime(p + 1, r.time);
	return p;
}

// Seconds since epoch (as if local time were UTC) at which the rule fires in `year`
static int64_t ruleLocalSecs(const Rule& r, int year){
	static const int MonthDays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	const int64_t first = daysFromCivil(year, r.month, 1);
	const int firstWday = (int) ((first + 4) % 7 + 7) % 7; // 1970-01-01 was a Thursday
	int day = 1 + (r.day - firstWday + 7) % 7 + (r.week - 1) * 7;
	int mdays = MonthDays[r.month - 1] + (r.month == 2 && isLeap(year));
	while(day > mdays) day -= 7;
	return (first + day - 1) * 86400 + r.time;
}

int16_t offsetAt(const char* tz, time_t utc){
	// std name, std offset (POSIX: positive = west of UTC)
	const char* p = skipName(tz);
	int32_t stdOff;
	p = parseTime(p, stdOff);
	stdOff = -stdOff;

	if(!*p) return (int16_t) (stdOff / 60);

	p = skipName(p);
	int32_t dstOff = stdOff + 3600;
	if(*p && *p != ','){
		p = parseTime(p, dstOff);
		dstOff = -dstOff;
	}
	if(*p != ',') return (int16_t) (stdOff / 60);

	Rule start, end;
	p = parseRule(p + 1, start);
	if(!p || *p != ',') return (int16_t) (stdOff / 60);
	p = parseRule(p + 1, end);
	if(!p) return (int16_t) (stdOff / 60);

	tm t;
	time_t local = utc + stdOff;
	gmtime_r(&local, &t);
	const int year = t.tm_year + 1900;

	// Transition instants in UTC: start is given in standard time, end in daylight time
	const int64_t startUtc = ruleLocalSecs(start, year) - stdOff;
	const int64_t endUtc = ruleLocalSecs(end, year) - dstOff;

	bool dst;
	if(startUtc < endUtc){
		dst = utc >= startUtc && utc < endUtc;		// northern hemisphere
	}else{
		dst = utc >= startUtc || utc < endUtc;		// southern hemisphere
	}
	return (int16_t) ((dst ? dstOff : stdOff) / 60);
}

tm localTime(const City& city, time_t utc, int16_t* offsetOut){
	const int16_t off = offsetAt(city.tz, utc);
	if(offsetOut) *offsetOut = off;
	time_t local = utc + (time_t) off * 60;
	tm t;
	gmtime_r(&local, &t);
	return t;
}

}
