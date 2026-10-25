#include "WorldClockScreen.h"
#include "Services/Time.h"
#include "Theme/theme.h"
#include "Util/Services.h"
#include "Util/TimeZones.h"
#include <cstdio>
#include <cstdlib>

static constexpr uint8_t None = 0xff;

WorldClockScreen::WorldClockScreen() : AppScreen(""){
	h24 = ((Settings*) Services.get(Service::Settings))->get().timeFormat24h;

	auto ext = (ExtSettings*) Services.get(Service::ExtSettings);
	const auto cfg = ext->get();
	for(int i = 0; i < ExtSettingsStruct::WorldClocks; i++){
		cities[i] = cfg.worldCities[i] < TimeZones::CityCount ? cfg.worldCities[i] : None;
	}
	// First run: a sensible default set
	if(cities[0] == None && cities[1] == None && cities[2] == None && cities[3] == None){
		cities[0] = 6;	// New York
		cities[1] = 16;	// London
		cities[2] = 35;	// Tokyo
		cities[3] = None;
	}

	homeTime = label(content, &lv_font_unscii_16, "");
	lv_obj_align(homeTime, LV_ALIGN_TOP_LEFT, 5, 4);
	homeInfo = label(content, &devin, "");
	lv_obj_set_style_text_align(homeInfo, LV_TEXT_ALIGN_RIGHT, 0);
	lv_obj_align(homeInfo, LV_ALIGN_TOP_RIGHT, -5, 3);

	for(int i = 0; i < ExtSettingsStruct::WorldClocks; i++){
		auto& r = rows[i];
		r.box = lv_obj_create(content);
		lv_obj_set_size(r.box, 122, 19);
		lv_obj_set_pos(r.box, 3, 24 + i * 20);
		lv_obj_set_style_border_color(r.box, textColor(), 0);
		lv_obj_set_style_border_width(r.box, 0, 0);
		lv_obj_clear_flag(r.box, LV_OBJ_FLAG_SCROLLABLE);

		r.name = label(r.box, &devin, "");
		lv_obj_align(r.name, LV_ALIGN_TOP_LEFT, 3, 2);
		r.diff = label(r.box, &devin, "", dimColor(), true);
		lv_obj_align(r.diff, LV_ALIGN_BOTTOM_LEFT, 3, -2);
		r.time = label(r.box, &lv_font_unscii_8, "");
		lv_obj_align(r.time, LV_ALIGN_RIGHT_MID, -3, 0);
	}

	footer = label(content, &devin, "");
	lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -4);

	refresh();
}

void WorldClockScreen::fmt(char* buf, size_t len, const tm& t) const{
	if(h24) snprintf(buf, len, "%02d:%02d", t.tm_hour, t.tm_min);
	else snprintf(buf, len, "%d:%02d%s", (t.tm_hour + 11) % 12 + 1, t.tm_min, t.tm_hour < 12 ? "am" : "pm");
}

void WorldClockScreen::refresh(){
	auto time = (Time*) Services.get(Service::Time);
	const tm local = time->getTime();
	const time_t utc = time->getUTC();
	const int16_t homeOff = time->getUtcOffset();

	char buf[32];
	fmt(buf, sizeof(buf), local);
	lv_label_set_text(homeTime, buf);
	snprintf(buf, sizeof(buf), "HOME\nUTC%+d%s", homeOff / 60, homeOff % 60 ? ":30" : "");
	lv_label_set_text(homeInfo, buf);
	lv_obj_align(homeInfo, LV_ALIGN_TOP_RIGHT, -5, 3);

	const int homeDay = local.tm_year * 400 + local.tm_yday;

	for(int i = 0; i < ExtSettingsStruct::WorldClocks; i++){
		auto& r = rows[i];
		lv_obj_set_style_border_width(r.box, (i == selected && (editing || lv_tick_get() < 0xffffffff)) ? (editing ? 2 : 1) : 0, 0);

		if(cities[i] == None){
			lv_label_set_text(r.name, editing && i == selected ? "(none)" : "-");
			lv_label_set_text(r.time, "");
			lv_label_set_text(r.diff, "");
			continue;
		}

		const auto& city = TimeZones::Cities[cities[i]];
		int16_t off;
		const tm t = TimeZones::localTime(city, utc, &off);
		lv_label_set_text(r.name, city.name);
		fmt(buf, sizeof(buf), t);
		lv_label_set_text(r.time, buf);

		const int diffMin = off - homeOff;
		const int day = t.tm_year * 400 + t.tm_yday;
		const char* dayTxt = day > homeDay ? " TOMORROW" : (day < homeDay ? " YESTERDAY" : "");
		if(diffMin == 0) snprintf(buf, sizeof(buf), "same time%s", dayTxt);
		else if(diffMin % 60) snprintf(buf, sizeof(buf), "%+d:%02dh%s", diffMin / 60, abs(diffMin % 60), dayTxt);
		else snprintf(buf, sizeof(buf), "%+dh%s", diffMin / 60, dayTxt);
		lv_label_set_text(r.diff, buf);
	}

	lv_label_set_text(footer, editing ? "UP/DOWN city  SEL done" : "hold SEL to edit");
}

void WorldClockScreen::cycleCity(int dir){
	// Cycle through the city list, with "none" between the last and first entries
	int c = cities[selected] == None ? (dir > 0 ? -1 : TimeZones::CityCount) : cities[selected];
	c += dir;
	cities[selected] = (c < 0 || c >= TimeZones::CityCount) ? None : (uint8_t) c;
	refresh();
}

void WorldClockScreen::save(){
	auto ext = (ExtSettings*) Services.get(Service::ExtSettings);
	auto cfg = ext->get();
	for(int i = 0; i < ExtSettingsStruct::WorldClocks; i++) cfg.worldCities[i] = cities[i];
	ext->set(cfg);
	ext->store();
}

void WorldClockScreen::onPress(Input::Button btn){
	if(btn != Input::Up && btn != Input::Down) return;
	if(editing){
		cycleCity(btn == Input::Up ? 1 : -1);
	}else{
		selected = (selected + ExtSettingsStruct::WorldClocks + (btn == Input::Up ? -1 : 1)) % ExtSettingsStruct::WorldClocks;
		refresh();
	}
}

void WorldClockScreen::onRepeat(Input::Button btn){
	if(editing) onPress(btn);
}

void WorldClockScreen::onRelease(Input::Button btn, uint32_t heldMs){
	if(btn == Input::Select && editing && heldMs < LongPressMs){
		editing = false;
		save();
		refresh();
	}
}

void WorldClockScreen::onLongPress(Input::Button btn){
	if(btn == Input::Select && !editing){
		editing = true;
		refresh();
	}
}

bool WorldClockScreen::onAlt(){
	if(!editing) return false;
	editing = false;
	save();
	refresh();
	return true;
}

void WorldClockScreen::tick(){
	static uint64_t last = 0;
	if(lv_tick_get() - last < 1000) return;
	last = lv_tick_get();
	refresh();
}
