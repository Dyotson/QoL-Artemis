#ifndef ARTEMIS_FIRMWARE_WEATHER_H
#define ARTEMIS_FIRMWARE_WEATHER_H

#include <nvs.h>
#include <mutex>
#include <string>
#include <ctime>
#include <cstdint>

/**
 * Weather from the iPhone. iOS doesn't share weather with Bluetooth accessories, so an
 * iPhone Shortcut gets the current weather and posts it as a notification:
 *
 *     ARTEMIS-WX|<Temperature>|<Condition>|<High>|<Low>
 *
 * The notification arrives over ANCS like any other; Phone hands it here instead of
 * showing it, and dismisses it on the iPhone. No Wi-Fi involved, so it costs the watch
 * nothing. The parser accepts what Shortcuts produces in any locale ("18°C", "64 °F",
 * "18,5 °C", localized condition names).
 */
class Weather {
public:
	Weather();

	static constexpr const char* Tag = "ARTEMIS-WX";

	enum class Icon : uint8_t { Unknown, Clear, ClearNight, PartlyCloudy, Cloudy, Fog, Drizzle, Rain, Snow, Storm, Wind };

	struct Data {
		bool valid = false;
		int16_t temp = 0;		// whole degrees, in `unit`
		int16_t high = 0;
		int16_t low = 0;
		bool hasHighLow = false;
		char unit = 'C';
		Icon icon = Icon::Unknown;
		char condition[28] = {};	// as sent by the phone (ASCII-sanitized)
		time_t updated = 0;		// local epoch (watch time) when received
	};

	/** True if the text carries a weather update (the tag) */
	static bool isWeatherText(const std::string& title, const std::string& message);

	/** Parse and store an update. Returns false if it couldn't be parsed. */
	bool ingest(const std::string& title, const std::string& message);

	Data get() const;

	/** Valid and not older than MaxAge */
	bool fresh() const;
	static constexpr time_t MaxAge = 6 * 60 * 60;

	/** e.g. "18°" (needs a font with the degree sign) */
	std::string tempText(bool withUnit = false) const;
	/** Minutes since the last update, -1 if none */
	int32_t ageMinutes() const;

	struct Event {
		enum { Updated } action;
	};

private:
	mutable std::mutex mut;
	Data data;
	nvs_handle_t handle{};

	static Icon iconFor(const char* condition, bool night);
	static bool parseNumber(const std::string& s, int16_t& out, char* unit = nullptr);
	void store();
};


#endif //ARTEMIS_FIRMWARE_WEATHER_H
