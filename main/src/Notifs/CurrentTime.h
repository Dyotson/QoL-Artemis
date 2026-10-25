#ifndef CLOCKSTAR_FIRMWARE_CURRENTTIME_H
#define CLOCKSTAR_FIRMWARE_CURRENTTIME_H

#include "BLE/Client.h"
#include "Util/Threaded.h"
#include "Util/PSRAMAllocator.h"

class CurrentTime : public Threaded {
public:
	CurrentTime(BLE::Client* client);

	/** Whether the phone offers Local Time Information (UTC offset), and the last raw value */
	bool hasLocalTimeInfo() const;
	int lastLtiRaw() const { return ltiRaw; }

private:
	std::shared_ptr<BLE::Client::Service> service;
	std::shared_ptr<BLE::Client::Char> chr;
	std::shared_ptr<BLE::Client::Char> ltiChr; // Local Time Information: UTC offset + DST

	void loop() override;

	void setTime(const PSRAMByteBuffer& data);
	void setLocalTimeInfo(const PSRAMByteBuffer& data);

	// The phone notifies on time changes, but re-read now and then anyway so the watch
	// clock (RTC drift) never wanders off.
	static constexpr uint64_t ResyncIntervalMs = 3ULL * 60 * 60 * 1000;
	uint64_t lastSync = 0;
	int ltiRaw = -1;
	uint8_t ltiAttempts = 0;
	uint64_t ltiLastRead = 0;

	static constexpr esp_bt_uuid_t ServiceUUID = { .len = ESP_UUID_LEN_16, .uuid = { .uuid16 = 0x1805 }};
	static constexpr esp_bt_uuid_t CharUUID = { .len = ESP_UUID_LEN_16, .uuid = { .uuid16 = 0x2A2B }};
	static constexpr esp_bt_uuid_t LTICharUUID = { .len = ESP_UUID_LEN_16, .uuid = { .uuid16 = 0x2A0F }};

};


#endif //CLOCKSTAR_FIRMWARE_CURRENTTIME_H
