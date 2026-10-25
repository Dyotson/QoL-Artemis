#include "CurrentTime.h"
#include <esp_log.h>
#include "Services/Time.h"
#include "Util/Services.h"
#include "Util/stdafx.h"

CurrentTime::CurrentTime(BLE::Client* client) : Threaded("CurrentTime", 3 * 1024){
	service = client->addService(ServiceUUID);
	chr = service->addChar(CharUUID, ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_READ);

	chr->setOnConnectedCb([this](){ chr->writeDescr(ESP_GATT_UUID_CHAR_CLIENT_CONFIG, { 0x01, 0x00 }); chr->read(); });

	ltiChr = service->addChar(LTICharUUID, ESP_GATT_CHAR_PROP_BIT_READ);
	ltiChr->setOnConnectedCb([this](){ ltiChr->read(); });

	start();
}

void CurrentTime::loop(){
	if(chr == nullptr || !chr->connected()){
		delayMillis(1000);
		return;
	}

	if(ltiChr->connected()){
		if(auto lti = ltiChr->getNextNotif(0)){
			setLocalTimeInfo(lti->data);
		}else if(ltiRaw < 0 && ltiAttempts < 6 && millis() - ltiLastRead > 3000){
			// The first read goes out together with the rest of the link setup and can get
			// lost; ask again a few times until the phone answers.
			ltiAttempts++;
			ltiLastRead = millis();
			ltiChr->read();
		}
	}else{
		ltiRaw = -1;
		ltiAttempts = 0;
	}

	if(lastSync != 0 && millis() - lastSync > ResyncIntervalMs){
		lastSync = millis();
		chr->read();
	}

	// Long wait: this only needs to wake for phone time updates (and the rare resync)
	auto notif = chr->getNextNotif(pdMS_TO_TICKS(ltiChr->connected() && ltiRaw < 0 ? 1000 : 60000));
	if(!notif) return;
	lastSync = millis();

	setTime(notif->data);

	// The phone notifies the current time on changes like DST transitions or travel:
	// refresh the UTC offset along with it.
	if(ltiChr->connected()){
		ltiChr->read();
	}
}

bool CurrentTime::hasLocalTimeInfo() const{
	return ltiChr && ltiChr->established();
}

void CurrentTime::setLocalTimeInfo(const PSRAMByteBuffer& data){
	if(data.size() < 2) return;
	ltiRaw = (data[0] << 8) | data[1];

	const int8_t tz = (int8_t) data[0];	// time zone, in 15 minute steps; -128 = unknown
	const uint8_t dst = data[1];		// 0, 2, 4, 8 = +0, +0.5h, +1h, +2h; 255 = unknown
	if(tz == -128) return;

	int16_t offset = tz * 15;
	switch(dst){
		case 2: offset += 30; break;
		case 4: offset += 60; break;
		case 8: offset += 120; break;
		default: break;
	}

	auto ts = static_cast<Time*>(Services.get(Service::Time));
	ts->setUtcOffset(offset, true);
}

void CurrentTime::setTime(const PSRAMByteBuffer& data){
	if(data.size() < 7) return;

	int year = data[0] | (data[1] << 8);
	int month = data[2];
	int day = data[3];
	int hour = data[4];
	int minute = data[5];
	int second = data[6];
	//int weekday = data[7];
	//int frac256 = data[8];

	tm time = {
			.tm_sec = second,
			.tm_min = minute,
			.tm_hour = hour,
			.tm_mday = day,
			.tm_mon = month-1,
			.tm_year = year - 1900
	};

	// Ignore obviously invalid values (e.g. an all-zero read)
	if(year < 2020 || month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 59) return;

	auto ts = static_cast<Time*>(Services.get(Service::Time));
	ts->setTime((tm) time);
	ESP_LOGI("CurrentTime", "Synced time from phone: %04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
}
