#ifndef ARTEMIS_FIRMWARE_EXTSETTINGS_H
#define ARTEMIS_FIRMWARE_EXTSETTINGS_H

#include <nvs.h>
#include <mutex>
#include <cstdint>

/**
 * Settings added on top of the stock firmware. Kept in a separate NVS blob so the
 * original Settings blob (which must end with the theme byte) stays untouched.
 *
 * Only ever APPEND fields to ExtSettingsStruct: on load, a shorter blob written by an
 * older firmware fills the prefix and the remaining fields keep their defaults.
 */

struct AlarmConfig {
	bool enabled = false;
	uint8_t hour = 7;
	uint8_t minute = 0;
	uint8_t days = 0x7f; // bit 0 = Sunday ... bit 6 = Saturday. 0 = ring once
};

enum class FaceComplication : uint8_t { None, Weather, Steps, WatchTemp };

struct ExtSettingsStruct {
	// Units & display
	bool fahrenheit = false;
	FaceComplication complication = FaceComplication::Weather;

	// Quiet hours: no beeps/LEDs for notifications between start and end (local hours)
	bool quietHours = false;
	uint8_t quietStart = 22;
	uint8_t quietEnd = 7;

	// Beep when the phone stays disconnected for a while
	bool disconnectAlert = false;

	// Time zones. homeOffset is used only when the phone doesn't report one.
	int16_t homeOffsetMin = 0;
	static constexpr uint8_t WorldClocks = 4;
	uint8_t worldCities[WorldClocks] = { 0xff, 0xff, 0xff, 0xff }; // indices into Cities, 0xff = empty

	// Wi-Fi used for weather updates
	char wifiSSID[33] = {};
	char wifiPass[65] = {};

	// Weather location. If autoLocation, it's derived from the Wi-Fi's public IP.
	bool weatherEnabled = true;
	bool autoLocation = true;
	float latitude = 0;
	float longitude = 0;
	char locationName[32] = {};
	uint8_t weatherIntervalMin = 60;

	// Alarms
	static constexpr uint8_t AlarmCount = 3;
	AlarmConfig alarms[AlarmCount] = {
			{ .enabled = false, .hour = 7, .minute = 0, .days = 0b0111110 },
			{ .enabled = false, .hour = 8, .minute = 30, .days = 0b1000001 },
			{ .enabled = false, .hour = 12, .minute = 0, .days = 0 },
	};

	// Activity
	uint32_t stepGoal = 8000;
	bool pedometer = true;
};

class ExtSettings {
public:
	ExtSettings();

	ExtSettingsStruct get();
	void set(const ExtSettingsStruct& settings);
	void store();

private:
	nvs_handle_t handle{};
	ExtSettingsStruct data;
	std::mutex mut;

	static constexpr const char* NVSNamespace = "ArtemisExt";
	static constexpr const char* BlobName = "Ext";

	void load();
};


#endif //ARTEMIS_FIRMWARE_EXTSETTINGS_H
