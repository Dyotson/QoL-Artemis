#include "Clocks.h"
#include "MelodyPlayer.h"
#include "SleepMan.h"
#include "StatusCenter.h"
#include "Services/Time.h"
#include "LV_Interface/LVGL.h"
#include "Screens/AlertScreen.h"
#include "Util/Events.h"
#include "Util/Services.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <algorithm>

static const char* TAG = "Clocks";

static uint64_t nowUs(){
	return esp_timer_get_time();
}

Clocks::Clocks() : Threaded("Clocks", 3 * 1024, 5), wakeSem(xSemaphoreCreateBinary()){
	start();
}

// ---------------------------------------------------------------- Stopwatch

void Clocks::swStartStop(){
	std::lock_guard lock(mut);
	if(swOn){
		swAccumUs += nowUs() - swStartUs;
		swOn = false;
	}else{
		swStartUs = nowUs();
		swOn = true;
	}
}

void Clocks::swLap(){
	std::lock_guard lock(mut);
	if(!swOn) return;
	const uint64_t t = (swAccumUs + nowUs() - swStartUs) / 1000;
	if(laps.size() >= MaxLaps) laps.erase(laps.begin());
	laps.push_back(t);
}

void Clocks::swReset(){
	std::lock_guard lock(mut);
	swOn = false;
	swAccumUs = 0;
	laps.clear();
}

bool Clocks::swRunning() const{
	std::lock_guard lock(mut);
	return swOn;
}

uint64_t Clocks::swElapsedMs() const{
	std::lock_guard lock(mut);
	return (swAccumUs + (swOn ? nowUs() - swStartUs : 0)) / 1000;
}

std::vector<uint64_t> Clocks::swLaps() const{
	std::lock_guard lock(mut);
	return laps;
}

// ---------------------------------------------------------------- Timer

void Clocks::timerSet(uint32_t seconds){
	std::lock_guard lock(mut);
	timerPresetS = std::clamp<uint32_t>(seconds, 1, 99 * 3600 + 59 * 60 + 59);
	if(!tmOn){
		tmPaused = false;
	}
}

uint32_t Clocks::timerPreset() const{
	std::lock_guard lock(mut);
	return timerPresetS;
}

void Clocks::timerStartPause(){
	{
		std::lock_guard lock(mut);
		if(!tmOn){
			tmOn = true;
			tmPaused = false;
			tmEndUs = nowUs() + (uint64_t) timerPresetS * 1000000;
		}else if(tmPaused){
			tmPaused = false;
			tmEndUs = nowUs() + (uint64_t) tmRemainingAtPause * 1000;
		}else{
			tmPaused = true;
			tmRemainingAtPause = (uint32_t) ((tmEndUs > nowUs() ? tmEndUs - nowUs() : 0) / 1000);
		}
	}
	xSemaphoreGive(wakeSem);
}

void Clocks::timerReset(){
	{
		std::lock_guard lock(mut);
		tmOn = false;
		tmPaused = false;
	}
	xSemaphoreGive(wakeSem);
}

bool Clocks::timerRunning() const{
	std::lock_guard lock(mut);
	return tmOn;
}

bool Clocks::timerPaused() const{
	std::lock_guard lock(mut);
	return tmOn && tmPaused;
}

uint32_t Clocks::timerRemainingMs() const{
	std::lock_guard lock(mut);
	if(!tmOn) return timerPresetS * 1000;
	if(tmPaused) return tmRemainingAtPause;
	const uint64_t now = nowUs();
	return tmEndUs > now ? (uint32_t) ((tmEndUs - now) / 1000) : 0;
}

// ---------------------------------------------------------------- Alarms

AlarmConfig Clocks::getAlarm(uint8_t i) const{
	auto ext = (ExtSettings*) Services.get(Service::ExtSettings);
	if(!ext || i >= ExtSettingsStruct::AlarmCount) return {};
	return ext->get().alarms[i];
}

void Clocks::setAlarm(uint8_t i, const AlarmConfig& cfg){
	auto ext = (ExtSettings*) Services.get(Service::ExtSettings);
	if(!ext || i >= ExtSettingsStruct::AlarmCount) return;
	auto s = ext->get();
	s.alarms[i] = cfg;
	ext->set(s);
	ext->store();
	xSemaphoreGive(wakeSem);
}

bool Clocks::alarmMatches(const AlarmConfig& a, const tm& now) const{
	if(!a.enabled || a.hour != now.tm_hour || a.minute != now.tm_min) return false;
	return a.days == 0 || (a.days & (1 << now.tm_wday));
}

int32_t Clocks::minutesToNextAlarm() const{
	auto time = (Time*) Services.get(Service::Time);
	auto ext = (ExtSettings*) Services.get(Service::ExtSettings);
	if(!time || !ext) return -1;

	const auto cfg = ext->get();
	const tm now = time->getTime();
	const int nowMin = now.tm_hour * 60 + now.tm_min;
	int32_t best = -1;

	for(const auto& a : cfg.alarms){
		if(!a.enabled) continue;
		for(int d = 0; d <= 7; d++){
			const int wday = (now.tm_wday + d) % 7;
			if(a.days != 0 && !(a.days & (1 << wday))) continue;
			const int32_t mins = d * 1440 + a.hour * 60 + a.minute - nowMin;
			if(mins <= 0) continue;
			if(best < 0 || mins < best) best = mins;
			break;
		}
	}
	return best;
}

// ---------------------------------------------------------------- Ringing

Clocks::Alert Clocks::currentAlert() const{
	std::lock_guard lock(mut);
	return alert;
}

void Clocks::ring(const Alert& a){
	{
		std::lock_guard lock(mut);
		alert = a;
		ringStartUs = nowUs();
	}
	ESP_LOGI(TAG, "Ringing (%s)", a.type == AlertType::Timer ? "timer" : "alarm");

	if(auto melody = (MelodyPlayer*) Services.get(Service::Melody)){
		size_t n;
		auto tone = MelodyPlayer::alarmTone(n);
		melody->play(tone, n, true);
	}
	if(auto status = (StatusCenter*) Services.get(Service::Status)){
		status->startAlarm();
	}

	// Show the alert screen: wake the display without the lock screen, then switch
	// screens on the UI thread (which runs again once the display is awake)
	if(auto sleep = (SleepMan*) Services.get(Service::Sleep)){
		sleep->wake(true);
	}
	if(auto lvgl = (LVGL*) Services.get(Service::LVGL)){
		lvgl->runOnUI([lvgl](){
			lvgl->startScreen([](){ return std::make_unique<AlertScreen>(); });
		});
	}

	Events::post(Facility::Clocks, Event{ .action = Event::Ring, .alert = a });
}

void Clocks::stopRinging(){
	Alert old;
	{
		std::lock_guard lock(mut);
		old = alert;
		alert = {};
	}
	if(old.type == AlertType::None) return;

	if(auto melody = (MelodyPlayer*) Services.get(Service::Melody)){
		melody->stop();
	}
	if(auto status = (StatusCenter*) Services.get(Service::Status)){
		status->stopAlarm();
	}
	Events::post(Facility::Clocks, Event{ .action = Event::Stopped, .alert = old });
}

void Clocks::dismiss(){
	stopRinging();
	xSemaphoreGive(wakeSem);
}

void Clocks::snooze(uint8_t minutes){
	const auto a = currentAlert();
	if(a.type == AlertType::Alarm){
		auto time = (Time*) Services.get(Service::Time);
		if(time){
			std::lock_guard lock(mut);
			snoozeUntilMinute = time->getLocalEpoch() / 60 + minutes;
			snoozeIndex = a.alarmIndex;
		}
	}else if(a.type == AlertType::Timer){
		timerSet(minutes * 60);
		timerStartPause();
	}
	stopRinging();
	xSemaphoreGive(wakeSem);
}

void Clocks::loop(){
	auto time = (Time*) Services.get(Service::Time);
	auto ext = (ExtSettings*) Services.get(Service::ExtSettings);
	if(!time || !ext){
		vTaskDelay(pdMS_TO_TICKS(1000));
		return;
	}

	const uint64_t now = nowUs();
	const time_t local = time->getLocalEpoch();
	const int64_t minute = local / 60;

	// Ring timeout
	bool ringing;
	{
		std::lock_guard lock(mut);
		ringing = alert.type != AlertType::None;
	}
	if(ringing && now - ringStartUs > RingTimeoutUs){
		stopRinging();
		ringing = false;
	}

	// Timer
	bool timerDone = false;
	{
		std::lock_guard lock(mut);
		if(tmOn && !tmPaused && now >= tmEndUs){
			tmOn = false;
			timerDone = true;
		}
	}
	if(timerDone && !ringing){
		ring({ .type = AlertType::Timer });
		ringing = true;
	}

	// Alarms (and snoozes), at most once per minute
	if(!ringing && minute != lastAlarmMinute){
		tm t;
		gmtime_r(&local, &t);
		auto cfg = ext->get();

		int8_t hit = -1;
		{
			std::lock_guard lock(mut);
			if(snoozeUntilMinute == minute){
				hit = snoozeIndex;
				snoozeUntilMinute = -1;
			}
		}
		for(uint8_t i = 0; hit < 0 && i < ExtSettingsStruct::AlarmCount; i++){
			if(alarmMatches(cfg.alarms[i], t)) hit = i;
		}

		if(hit >= 0){
			lastAlarmMinute = minute;
			if(cfg.alarms[hit].days == 0 && cfg.alarms[hit].enabled){
				// One-shot alarm: disable it once it rang
				cfg.alarms[hit].enabled = false;
				ext->set(cfg);
				ext->store();
			}
			ring({ .type = AlertType::Alarm, .alarmIndex = (uint8_t) hit, .hour = (uint8_t) t.tm_hour, .minute = (uint8_t) t.tm_min });
		}
	}

	// Sleep until the next thing can happen: timer end, start of the next minute if an
	// alarm/snooze is pending, ring timeout. Capped so clock changes are picked up.
	uint64_t waitMs = 10 * 60 * 1000;
	{
		std::lock_guard lock(mut);
		if(tmOn && !tmPaused){
			waitMs = std::min<uint64_t>(waitMs, tmEndUs > now ? (tmEndUs - now) / 1000 + 1 : 1);
		}
		if(alert.type != AlertType::None){
			waitMs = std::min<uint64_t>(waitMs, (ringStartUs + RingTimeoutUs - std::min(now, ringStartUs + RingTimeoutUs)) / 1000 + 10);
		}
	}
	const int32_t nextAlarm = minutesToNextAlarm();
	const bool snoozing = snoozeUntilMinute >= 0;
	if(nextAlarm >= 0 || snoozing){
		// Wake at the start of the minute in which the alarm is due
		const uint64_t toNextMinute = (60 - (local % 60)) * 1000 + 300;
		const uint64_t toAlarm = (nextAlarm > 1 && !snoozing) ? toNextMinute + (uint64_t) (nextAlarm - 1) * 60000 : toNextMinute;
		waitMs = std::min(waitMs, toAlarm);
	}

	xSemaphoreTake(wakeSem, pdMS_TO_TICKS(std::max<uint64_t>(waitMs, 5)));
}
