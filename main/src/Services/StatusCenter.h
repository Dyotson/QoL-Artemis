#ifndef CLOCKSTAR_FIRMWARE_STATUSCENTER_H
#define CLOCKSTAR_FIRMWARE_STATUSCENTER_H


#include "Util/Threaded.h"
#include <atomic>
#include "Util/Events.h"
#include "Notifs/Phone.h"
#include "Devices/Battery.h"
#include "ChirpSystem.h"
#include "Settings/Settings.h"
#include "Devices/LEDController.h"
#include <esp_timer.h>

class StatusCenter : public Threaded {
public:
	StatusCenter();
	~StatusCenter();

	void blockAudio(bool block);

	void updateLED();
	void blink();
	void circularBlink();
	void beep();
	void shutdown();

	void blinkUp();
	void blinkDown();
	void blinkRand();
	void blinkAll();
	void blinkAllTwice();

	/** Repeating attention pattern for alarms/timers. Stops on stopAlarm(). */
	void startAlarm();
	void stopAlarm();

	/** Battery LED state, for diagnostics */
	const char* ledStateName() const;

	/** Whether notification alerts are currently muted by quiet hours */
	bool inQuietHours();

private:
	EventQueue events;

	ChirpSystem& chirp;
	Settings& settings;
	RGBLEDController* led;

	std::array<std::shared_ptr<DigitalLEDController>, 6> singleLeds;

	bool audioBlocked = false;
	bool hasNotifs = false;
	enum { Ok, Empty, Charging, Full } battState = Ok;

	void loop() override;

	void processPhone(const Phone::Event& evt);
	void processBatt(const Battery::Event& evt);

	// "Phone lost" alert: fires if the phone stays disconnected for LostAlertMs
	static constexpr uint64_t LostAlertMs = 60 * 1000;
	esp_timer_handle_t lostTimer = nullptr;
	void onPhoneLost();
	void showCall();

	esp_timer_handle_t alarmTimer = nullptr;
	std::atomic<bool> alarmActive{ false };
	void alarmTick();

};


#endif //CLOCKSTAR_FIRMWARE_STATUSCENTER_H
