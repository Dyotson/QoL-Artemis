#include "WeatherElement.h"
#include "WeatherIcons.h"
#include "Services/Weather.h"
#include "Theme/theme.h"
#include "Util/Services.h"
#include "Util/stdafx.h"

const lv_font_t* WeatherElement::font(){
	static lv_font_t withDegree = [](){
		lv_font_t f = landerfont;
		f.fallback = &degree_font;
		return f;
	}();
	return &withDegree;
}

WeatherElement::WeatherElement(lv_obj_t* parent, lv_color_t color) : LVObject(parent){
	lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_column(obj, 2, 0);
	lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

	icon = lv_img_create(obj);
	lv_obj_set_style_img_recolor(icon, color, 0);
	lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER, 0);

	temp = lv_label_create(obj);
	lv_obj_set_style_text_font(temp, font(), 0);
	lv_obj_set_style_text_color(temp, color, 0);

	refresh();
}

void WeatherElement::refresh(){
	auto weather = (Weather*) Services.get(Service::Weather);
	if(!weather || !weather->fresh()){
		lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
		shownUpdate = -1;
		return;
	}

	const auto d = weather->get();
	shownUpdate = d.updated;
	lv_img_set_src(icon, WeatherIcons12[(int) d.icon]);
	lv_label_set_text(temp, weather->tempText().c_str());
	lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

void WeatherElement::loop(){
	if(millis() - lastCheck < 1000) return;
	lastCheck = millis();

	auto weather = (Weather*) Services.get(Service::Weather);
	if(!weather) return;
	const auto d = weather->get();
	const bool visible = !lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
	if(d.updated != shownUpdate || visible != weather->fresh()){
		refresh();
	}
}
