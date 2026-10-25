#include <esp_log.h>
#include "Time.h"
#include "Util/stdafx.h"
#include "Util/Events.h"
#include "Util/Services.h"
#include "Settings/ExtSettings.h"

static const char* TAG = "Time";

Time::Time(RTC& rtc) : SleepyThreaded(UpdateInterval, "Time", 3 * 1024, 6, 0), rtc(rtc){
	if(auto ext = (ExtSettings*) Services.get(Service::ExtSettings)){
		utcOffset = ext->get().homeOffsetMin;
	}
	updateFromRTC();
	start();
}

Time::~Time(){
	stop();
}

std::tm Time::getTime() const{
	time_t currentTime = time + (millis() - updateTime) / 1000;
	tm ret = {};
	gmtime_r(&currentTime, &ret);
	return ret;
}

void Time::setTime(tm time_tm){
	ESP_LOGI(TAG, "Updating time by tm");

	resetTime();
	updateTime = millis();

	Time::time = mktime(&time_tm); // also fills in tm_wday, which the RTC stores
	rtc.setTime(time_tm);

	Events::post(Facility::Time, Time::Event { .action = Event::Updated, .updated = { .time = time_tm } });
}

void Time::setTime(time_t time){
	ESP_LOGI(TAG, "Updating time by time_t");

	resetTime();
	updateTime = millis();

	tm time_tm = {};
	gmtime_r(&time, &time_tm);

	rtc.setTime(time_tm);
	Time::time = time;

	Events::post(Facility::Time, Time::Event { .action = Event::Updated, .updated = { .time = time_tm } });
}

tm Time::updateFromRTC(){
	resetTime();
	updateTime = millis();

	tm time_tm = rtc.getTime();
	time = mktime(&time_tm);
	return time_tm;
}

void Time::sleepyLoop(){
	ESP_LOGI(TAG, "Scheduled RTC sync");
	auto time_tm = updateFromRTC();
	Events::post(Facility::Time, Time::Event { .action = Event::Updated, .updated = { .time = time_tm } });
}

time_t Time::getLocalEpoch() const{
	return time + (millis() - updateTime) / 1000;
}

time_t Time::getUTC() const{
	return getLocalEpoch() - (time_t) utcOffset * 60;
}

int16_t Time::getUtcOffset() const{
	return utcOffset;
}

bool Time::utcOffsetKnown() const{
	return offsetFromPhone;
}

void Time::setUtcOffset(int16_t minutes, bool fromPhone){
	if(fromPhone) offsetFromPhone = true;
	if(minutes == utcOffset) return;
	utcOffset = minutes;

	// Remember it, so world clocks are right even before the phone reconnects
	if(auto ext = (ExtSettings*) Services.get(Service::ExtSettings)){
		auto cfg = ext->get();
		cfg.homeOffsetMin = minutes;
		ext->set(cfg);
		ext->store();
	}
	ESP_LOGI(TAG, "UTC offset %d min (%s)", minutes, fromPhone ? "phone" : "manual");
}
