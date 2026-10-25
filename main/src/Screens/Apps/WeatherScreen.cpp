#include "WeatherScreen.h"
#include "Devices/IMU.h"
#include "Services/Weather.h"
#include "Theme/theme.h"
#include "UIElements/WeatherElement.h"
#include "UIElements/WeatherIcons.h"
#include "Util/Services.h"
#include "Util/stdafx.h"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>

WeatherScreen::WeatherScreen() : AppScreen(""){
	icon = lv_img_create(content);
	lv_obj_set_style_img_recolor(icon, textColor(), 0);
	lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER, 0);
	lv_obj_set_pos(icon, 8, 8);

	temp = label(content, &clockfont, "");
	lv_obj_set_pos(temp, 40, 4);
	unitLabel = label(content, WeatherElement::font(), "");

	cond = label(content, WeatherElement::font(), "");
	lv_label_set_long_mode(cond, LV_LABEL_LONG_SCROLL_CIRCULAR);
	lv_obj_set_width(cond, 118);
	lv_obj_set_pos(cond, 6, 37);

	hilo = label(content, WeatherElement::font(), "");
	lv_obj_set_pos(hilo, 6, 50);

	age = label(content, &devin, "", dimColor(), true);
	lv_obj_set_pos(age, 6, 62);

	auto line = lv_obj_create(content);
	lv_obj_set_size(line, 116, 1);
	lv_obj_set_pos(line, 6, 75);
	lv_obj_set_style_bg_color(line, dimColor(), 0);
	lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);

	sensor = label(content, WeatherElement::font(), "");
	lv_obj_set_pos(sensor, 6, 81);

	help = label(content, &devin, "", dimColor(), true);
	lv_obj_set_pos(help, 6, 93);

	refresh();
}

void WeatherScreen::refresh(){
	auto weather = (Weather*) Services.get(Service::Weather);
	const auto d = weather->get();
	shownUpdate = d.updated;

	if(!d.valid){
		lv_img_set_src(icon, WeatherIcons24[0]);
		lv_label_set_text(temp, "--");
		lv_label_set_text(unitLabel, "");
		lv_label_set_text(cond, "NO WEATHER YET");
		lv_label_set_text(hilo, "");
		lv_label_set_text(age, "Set up the iPhone Shortcut");
		return;
	}

	char buf[48];
	lv_img_set_src(icon, WeatherIcons24[(int) d.icon]);
	snprintf(buf, sizeof(buf), "%d", d.temp);
	lv_label_set_text(temp, buf); // the big font has no degree sign; it goes in a small label
	snprintf(buf, sizeof(buf), "\xC2\xB0%c", d.unit);
	lv_label_set_text(unitLabel, buf);
	lv_obj_update_layout(temp);
	lv_obj_align_to(unitLabel, temp, LV_ALIGN_OUT_RIGHT_TOP, 2, 3);

	// Face fonts only have capitals
	std::string c = d.condition[0] ? d.condition : "-";
	for(auto& ch : c) ch = (char) toupper((unsigned char) ch);
	lv_label_set_text(cond, c.c_str());

	if(d.hasHighLow) snprintf(buf, sizeof(buf), "H %d\xC2\xB0  L %d\xC2\xB0", d.high, d.low);
	else buf[0] = 0;
	lv_label_set_text(hilo, buf);

	const int32_t mins = weather->ageMinutes();
	if(mins < 1) snprintf(buf, sizeof(buf), "updated just now");
	else if(mins < 60) snprintf(buf, sizeof(buf), "updated %ld min ago", mins);
	else snprintf(buf, sizeof(buf), "updated %ldh %02ldm ago%s", mins / 60, mins % 60, weather->fresh() ? "" : " (old)");
	lv_label_set_text(age, buf);
}

void WeatherScreen::tick(){
	auto weather = (Weather*) Services.get(Service::Weather);
	if(weather->get().updated != shownUpdate) refresh();

	if(millis() - lastSensor < 2000) return;
	lastSensor = millis();
	if(lastSensor > 2000) refresh(); // keep the "updated x min ago" current

	auto imu = (IMU*) Services.get(Service::IMU);
	const float t = imu ? imu->getTemperature() : NAN;
	const char unit = weather->get().unit;
	char buf[40];
	if(std::isnan(t)){
		snprintf(buf, sizeof(buf), "WATCH SENSOR --");
	}else{
		const float v = unit == 'F' ? t * 9 / 5 + 32 : t;
		snprintf(buf, sizeof(buf), "WATCH SENSOR %d.%d\xC2\xB0%c", (int) v, abs((int) (v * 10)) % 10, unit);
	}
	lv_label_set_text(sensor, buf);
	lv_label_set_text(help, "(skin temp while worn)");
}
