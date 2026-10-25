#ifndef ARTEMIS_FIRMWARE_CLOCKS_H
#define ARTEMIS_FIRMWARE_CLOCKS_H

#include "Util/Threaded.h"
#include "Settings/ExtSettings.h"
#include <atomic>
#include <mutex>
#include <vector>
#include <cstdint>
#include <freertos/semphr.h>

/**
 * Stopwatch, countdown timer and alarms. State lives here (not in the screens) so it
 * keeps going when you leave the app or the screen turns off.
 *
 * Battery: the service task sleeps until the next deadline (timer end or next alarm
 * minute) instead of polling, so it costs nothing while idle.
 */
class Clocks : private Threaded {
public:
	Clocks();

	// ---- Stopwatch ----
	void swStartStop();
	void swLap();
	void swReset();
	bool swRunning() const;
	uint64_t swElapsedMs() const;
	std::vector<uint64_t> swLaps() const; // lap split times [ms], newest last

	// ---- Timer ----
	void timerSet(uint32_t seconds);
	uint32_t timerPreset() const;
	void timerStartPause();
	void timerReset();
	bool timerRunning() const;
	bool timerPaused() const;
	uint32_t timerRemainingMs() const;

	// ---- Alarms ----
	AlarmConfig getAlarm(uint8_t i) const;
	void setAlarm(uint8_t i, const AlarmConfig& cfg);
	/** Minutes until the next enabled alarm rings, or -1 if none */
	int32_t minutesToNextAlarm() const;

	// ---- Ringing ----
	enum class AlertType : uint8_t { None, Timer, Alarm };
	struct Alert {
		AlertType type = AlertType::None;
		uint8_t alarmIndex = 0;
		uint8_t hour = 0, minute = 0;
	};
	Alert currentAlert() const;
	void dismiss();
	void snooze(uint8_t minutes = 5);

	struct Event {
		enum { Ring, Stopped } action;
		Alert alert;
	};

private:
	mutable std::mutex mut;
	SemaphoreHandle_t wakeSem;

	// Stopwatch
	bool swOn = false;
	uint64_t swStartUs = 0;		// when the current run started
	uint64_t swAccumUs = 0;		// time from previous runs
	std::vector<uint64_t> laps;
	static constexpr size_t MaxLaps = 30;

	// Timer
	uint32_t timerPresetS = 5 * 60;
	bool tmOn = false;
	bool tmPaused = false;
	uint64_t tmEndUs = 0;
	uint32_t tmRemainingAtPause = 0; // [ms]

	// Alarms
	int64_t lastAlarmMinute = -1;	// local epoch minute we last rang for (avoid double rings)
	int64_t snoozeUntilMinute = -1;
	uint8_t snoozeIndex = 0;

	Alert alert;
	uint64_t ringStartUs = 0;
	static constexpr uint64_t RingTimeoutUs = 60ULL * 1000 * 1000; // stop ringing after a minute

	void loop() override;
	void ring(const Alert& a);
	void stopRinging();
	bool alarmMatches(const AlarmConfig& a, const tm& now) const;
};


#endif //ARTEMIS_FIRMWARE_CLOCKS_H
