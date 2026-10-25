#include "Weather.h"
#include "Services/Time.h"
#include "Util/Events.h"
#include "Util/Services.h"
#include "Util/TextSanitize.h"
#include <esp_log.h>
#include <cctype>
#include <cmath>
#include <cstring>
#include <vector>

static const char* TAG = "Weather";

Weather::Weather(){
	const esp_timer_create_args_t args = {
			.callback = [](void* arg){ static_cast<Weather*>(arg)->store(); },
			.arg = this,
			.dispatch_method = ESP_TIMER_TASK,
			.name = "WxStore",
			.skip_unhandled_events = true
	};
	esp_timer_create(&args, &storeTimer);

	if(nvs_open("ArtemisWx", NVS_READWRITE, &handle) != ESP_OK) return;

	size_t size = sizeof(Data);
	Data stored;
	if(nvs_get_blob(handle, "Last", &stored, &size) == ESP_OK && size == sizeof(Data)){
		stored.condition[sizeof(stored.condition) - 1] = 0;
		data = stored;
	}
}

static bool containsNoCase(const std::string& hay, const char* needle){
	std::string h(hay), n(needle);
	for(auto& c : h) c = (char) toupper((unsigned char) c);
	for(auto& c : n) c = (char) toupper((unsigned char) c);
	return h.find(n) != std::string::npos;
}

// "24C|Nublado|24C|8C", "18 °C|Partly Cloudy|...", "-3|Snow": a number first, then '|'-separated fields
static bool looksLikeWeather(const std::string& text){
	size_t i = 0;
	while(i < text.size() && text[i] == ' ') i++;
	if(i < text.size() && text[i] == '-') i++;
	if(i >= text.size() || !isdigit((unsigned char) text[i])) return false;
	const size_t bar = text.find('|');
	return bar != std::string::npos && bar < 12 && bar + 1 < text.size();
}

bool Weather::isWeatherNotif(const std::string& appID, const std::string& title, const std::string& message){
	if(containsNoCase(title, Tag) || containsNoCase(message, Tag)) return true;
	return appID == "com.apple.shortcuts" && looksLikeWeather(message);
}

bool Weather::parseNumber(const std::string& s, int16_t& out, char* unit){
	// First number in the string; accepts "-3", "18.5", "18,5", "−3" (unicode minus is
	// sanitized away to ASCII by then, so a lone '-' before digits is the sign)
	size_t i = 0;
	while(i < s.size() && !isdigit((unsigned char) s[i])) i++;
	if(i == s.size()) return false;

	const bool neg = i > 0 && s[i - 1] == '-';
	float v = 0;
	while(i < s.size() && isdigit((unsigned char) s[i])){
		v = v * 10 + (s[i] - '0');
		i++;
	}
	if(i + 1 < s.size() && (s[i] == '.' || s[i] == ',') && isdigit((unsigned char) s[i + 1])){
		v += (s[i + 1] - '0') / 10.0f;
		i += 2;
		while(i < s.size() && isdigit((unsigned char) s[i])) i++;
	}
	out = (int16_t) lroundf(neg ? -v : v);

	if(unit){
		for(; i < s.size(); i++){
			const char c = (char) toupper((unsigned char) s[i]);
			if(c == 'F' || c == 'C'){
				*unit = c;
				break;
			}
		}
	}
	return true;
}

Weather::Icon Weather::iconFor(const char* condition, bool night){
	std::string c(condition);
	for(auto& ch : c) ch = (char) tolower((unsigned char) ch);
	auto has = [&c](const char* word){ return c.find(word) != std::string::npos; };

	// English and Spanish condition names (Shortcuts localizes them)
	if(has("thunder") || has("storm") || has("tormenta") || has("electrica")) return Icon::Storm;
	if(has("snow") || has("sleet") || has("flurr") || has("nieve") || has("nevad") || has("aguanieve") || has("blizzard")) return Icon::Snow;
	if(has("drizzle") || has("llovizna")) return Icon::Drizzle;
	if(has("rain") || has("shower") || has("lluvia") || has("chubasco") || has("aguacero")) return Icon::Rain;
	if(has("fog") || has("mist") || has("haze") || has("smoke") || has("niebla") || has("neblina") || has("bruma") || has("calima") || has("humo")) return Icon::Fog;
	if(has("wind") || has("breez") || has("viento") || has("ventoso")) return Icon::Wind;
	if(has("partly") || has("mostly sunny") || has("mostly clear") || has("parcial") || has("intervalos") || has("algo nublado") || has("mayormente despejado")) return Icon::PartlyCloudy;
	if(has("cloud") || has("overcast") || has("nublado") || has("nubes") || has("cubierto")) return Icon::Cloudy;
	if(has("clear") || has("sunny") || has("fair") || has("despejado") || has("soleado")) return night ? Icon::ClearNight : Icon::Clear;
	return Icon::Unknown;
}

bool Weather::ingest(const std::string& title, const std::string& message, int8_t postedHour, int8_t postedMinute){
	// Fields follow the tag if there is one (in the message, or in the title with the
	// fields in the message), otherwise the message is just the fields
	std::string text;
	std::string upperMsg(message), upperTitle(title);
	for(auto& c : upperMsg) c = (char) toupper((unsigned char) c);
	for(auto& c : upperTitle) c = (char) toupper((unsigned char) c);
	if(const size_t tag = upperMsg.find(Tag); tag != std::string::npos){
		text = message.substr(tag + strlen(Tag));
	}else if(const size_t tagT = upperTitle.find(Tag); tagT != std::string::npos){
		text = title.substr(tagT + strlen(Tag)) + "|" + message;
	}else{
		text = message;
	}

	std::vector<std::string> fields;
	size_t start = 0;
	while(start <= text.size()){
		size_t end = text.find_first_of("|;\n", start);
		if(end == std::string::npos) end = text.size();
		std::string f = text.substr(start, end - start);
		f.erase(0, f.find_first_not_of(" \t"));
		f.erase(f.find_last_not_of(" \t") + 1);
		if(!f.empty()) fields.push_back(f);
		start = end + 1;
	}
	if(fields.empty()) return false;

	Data d;
	if(!parseNumber(fields[0], d.temp, &d.unit)) return false;

	if(fields.size() > 1){
		const std::string cond = sanitizeToAscii(fields[1]);
		strncpy(d.condition, cond.c_str(), sizeof(d.condition) - 1);
	}
	if(fields.size() > 3){
		d.hasHighLow = parseNumber(fields[2], d.high) && parseNumber(fields[3], d.low);
	}

	auto time = (Time*) Services.get(Service::Time);
	const tm now = time ? time->getTime() : tm{};
	const bool night = now.tm_hour < 7 || now.tm_hour >= 20;
	d.icon = iconFor(d.condition, night);
	d.updated = time ? time->getLocalEpoch() : 0;
	if(time && postedHour >= 0 && postedMinute >= 0){
		// Today at the posting time, or yesterday if that's in the future
		const time_t now = d.updated;
		time_t posted = now - (now % 86400) + postedHour * 3600 + postedMinute * 60;
		if(posted > now + 60) posted -= 86400;
		d.updated = posted;
	}
	d.valid = true;

	{
		std::lock_guard lock(mut);
		if(data.valid && data.updated > d.updated) return true; // we already have something newer
		data = d;
	}
	if(storeTimer){
		esp_timer_stop(storeTimer);
		esp_timer_start_once(storeTimer, 2 * 1000 * 1000); // also coalesces bursts of re-sent updates
	}

	ESP_LOGI(TAG, "Weather: %d%c %s (H %d L %d)", d.temp, d.unit, d.condition, d.high, d.low);
	Events::post(Facility::Weather, Event{ .action = Event::Updated });
	return true;
}

void Weather::store(){
	std::lock_guard lock(mut);
	nvs_set_blob(handle, "Last", &data, sizeof(Data));
	nvs_commit(handle);
}

Weather::Data Weather::get() const{
	std::lock_guard lock(mut);
	return data;
}

int32_t Weather::ageMinutes() const{
	auto time = (Time*) Services.get(Service::Time);
	std::lock_guard lock(mut);
	if(!data.valid || !time) return -1;
	const time_t now = time->getLocalEpoch();
	return now >= data.updated ? (int32_t) ((now - data.updated) / 60) : 0;
}

bool Weather::fresh() const{
	const int32_t age = ageMinutes();
	return age >= 0 && age * 60 <= MaxAge;
}

std::string Weather::tempText(bool withUnit) const{
	const auto d = get();
	if(!d.valid) return "--";
	char buf[16];
	if(withUnit) snprintf(buf, sizeof(buf), "%d\xC2\xB0%c", d.temp, d.unit);
	else snprintf(buf, sizeof(buf), "%d\xC2\xB0", d.temp);
	return buf;
}
