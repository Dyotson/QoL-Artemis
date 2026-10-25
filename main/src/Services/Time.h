#ifndef CLOCKSTAR_FIRMWARE_TIME_H
#define CLOCKSTAR_FIRMWARE_TIME_H

#include "Util/Threaded.h"
#include "Devices/RTC.h"

class Time : public SleepyThreaded {
public:
	Time(RTC& rtc);
	~Time();

	struct Event {
		enum { Updated } action;
		union {
			struct {
				tm time;
			} updated;
		};
	};

	tm getTime() const;
	void setTime(tm time_tm);
	void setTime(time_t time);

	/** Local time as a time_t (the watch keeps local time; this is not a UTC timestamp) */
	time_t getLocalEpoch() const;
	/** Real UTC timestamp, using the phone-reported (or manually set) UTC offset */
	time_t getUTC() const;

	/** Offset of local time from UTC in minutes, e.g. -180 for UTC-3. */
	int16_t getUtcOffset() const;
	void setUtcOffset(int16_t minutes, bool fromPhone);
	/** Whether the offset came from the phone (otherwise it's the manual setting) */
	bool utcOffsetKnown() const;

private:
	RTC& rtc;

	static constexpr uint32_t UpdateInterval = 5000; // [ms]
	uint64_t updateTime = 0;

	time_t time;
	int16_t utcOffset = 0;
	bool offsetFromPhone = false;

	void sleepyLoop() override;
	tm updateFromRTC();

	// Hide public functions
	using Threaded::start;
	using Threaded::stop;

};


#endif //CLOCKSTAR_FIRMWARE_TIME_H
