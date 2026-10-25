#include "AlertScreen.h"
#include "Devices/Input.h"
#include "Services/Clocks.h"
#include "Services/SleepMan.h"
#include "Settings/Settings.h"
#include "Screens/Lock/LockScreen.h"
#include "Theme/theme.h"
#include "Util/Services.h"
#include "Util/stdafx.h"
#include "LV_Interface/InputLVGL.h"
#include <cstdio>

AlertScreen::AlertScreen() : queue(8), shownAt(millis()){
	auto settings = (Settings*) Services.get(Service::Settings);
	const auto theme = settings->get().themeData;
	const auto alert = ((Clocks*) Services.get(Service::Clocks))->currentAlert();

	lv_obj_set_size(*this, 128, 128);
	lv_obj_set_style_bg_color(*this, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(*this, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(*this, theme.primaryColor, 0);
	lv_obj_set_style_border_width(*this, 2, 0);
	lv_obj_set_flex_flow(*this, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(*this, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_row(*this, 6, 0);

	const lv_color_t color = theme.theme == Theme::Theme4 ? theme.backgroundColor : theme.primaryColor;

	title = lv_label_create(*this);
	lv_obj_set_style_text_font(title, &lv_font_unscii_16, 0);
	lv_obj_set_style_text_color(title, color, 0);
	lv_label_set_text(title, alert.type == Clocks::AlertType::Timer ? "TIMER" : "ALARM");

	big = lv_label_create(*this);
	lv_obj_set_style_text_font(big, &clockfont, 0);
	lv_obj_set_style_text_color(big, color, 0);
	char buf[24];
	if(alert.type == Clocks::AlertType::Alarm){
		snprintf(buf, sizeof(buf), "%02d:%02d", alert.hour, alert.minute);
	}else{
		snprintf(buf, sizeof(buf), "00:00");
	}
	lv_label_set_text(big, buf);

	auto hint = lv_label_create(*this);
	lv_obj_set_style_text_font(hint, &devin, 0);
	lv_obj_set_style_text_color(hint, color, 0);
	lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
	lv_label_set_text(hint, alert.type == Clocks::AlertType::Timer ? "SEL stop\nUP/DOWN +5 min" : "SEL stop\nUP/DOWN snooze 5 min");
}

AlertScreen::~AlertScreen(){
	Events::unlisten(&queue);
}

void AlertScreen::onStart(){
	lv_indev_wait_release(InputLVGL::getInstance()->getIndev());
	queue.reset();
	Events::listen(Facility::Input, &queue);
	Events::listen(Facility::Clocks, &queue);
	if(auto sleep = (SleepMan*) Services.get(Service::Sleep)) sleep->enAutoSleep(false);
}

void AlertScreen::onStop(){
	Events::unlisten(&queue);
	if(auto sleep = (SleepMan*) Services.get(Service::Sleep)) sleep->enAutoSleep(true);
}

void AlertScreen::close(){
	transition([](){ return std::make_unique<LockScreen>(); });
}

void AlertScreen::loop(){
	auto clocks = (Clocks*) Services.get(Service::Clocks);

	// Blink the big text so it's noticeable
	lv_obj_set_style_opa(big, ((millis() - shownAt) / 400) % 2 ? LV_OPA_40 : LV_OPA_COVER, 0);

	Event evt;
	while(queue.get(evt, 0)){
		if(evt.facility == Facility::Input){
			auto data = (Input::Data*) evt.data;
			const bool press = data->action == Input::Data::Press;
			const auto btn = data->btn;
			free(evt.data);
			// Ignore presses in the first moment, so the button that woke the watch doesn't dismiss
			if(!press || millis() - shownAt < 400) continue;

			if(btn == Input::Up || btn == Input::Down){
				clocks->snooze(5);
			}else{
				clocks->dismiss();
			}
			close();
			return;
		}else if(evt.facility == Facility::Clocks){
			auto data = (Clocks::Event*) evt.data;
			const bool stopped = data->action == Clocks::Event::Stopped;
			free(evt.data);
			if(stopped){ // rang out
				close();
				return;
			}
		}else{
			free(evt.data);
		}
	}
}
