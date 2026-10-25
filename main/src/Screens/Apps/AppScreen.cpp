#include "AppScreen.h"
#include "Filepaths.hpp"
#include "Theme/theme.h"
#include "Util/Services.h"
#include "Util/stdafx.h"
#include "LV_Interface/InputLVGL.h"
#include "Screens/MainMenu/MainMenu.h"

AppScreen::AppScreen(const char* title) : queue(8){
	auto settings = (Settings*) Services.get(Service::Settings);
	theme = settings->get().themeData;
	rotated = settings->get().screenRotate;

	lv_obj_set_size(*this, 128, 128);
	lv_obj_clear_flag(*this, LV_OBJ_FLAG_SCROLLABLE);

	bg = lv_obj_create(*this);
	lv_obj_set_size(bg, 128, 128);
	lv_obj_set_pos(bg, 0, 0);
	lv_obj_set_style_bg_color(bg, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
	lv_obj_set_style_bg_img_src(bg, THEMED_FILE(Menu, Background, theme.theme), 0);
	lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

	content = lv_obj_create(*this);
	lv_obj_set_size(content, 128, ContentH);
	lv_obj_set_pos(content, 0, Top);
	lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_pad_all(content, 0, 0);

	statusBar = new StatusBar(*this);
	lv_obj_set_pos(*statusBar, 0, 0);

	if(title && *title){
		auto t = label(content, &landerfont, title);
		lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 3);
	}
}

AppScreen::~AppScreen(){
	Events::unlisten(&queue);
}

lv_color_t AppScreen::textColor() const{
	// Themes 4/5 draw dark text on light panels via primaryColor; the rest use it on black
	if(theme.theme == Theme::Theme5) return theme.secondaryColor;
	return theme.primaryColor;
}

lv_color_t AppScreen::dimColor() const{
	return lv_color_mix(textColor(), theme.theme == Theme::Theme4 ? lv_color_white() : lv_color_black(), 150);
}

lv_obj_t* AppScreen::label(lv_obj_t* parent, const lv_font_t* font, const char* text, lv_color_t color, bool useColor){
	auto l = lv_label_create(parent);
	lv_obj_set_style_text_font(l, font, 0);
	lv_obj_set_style_text_color(l, useColor ? color : textColor(), 0);
	lv_label_set_text(l, text);
	return l;
}

void AppScreen::backToMenu(){
	transition([](){ return std::make_unique<MainMenu>(); });
}

void AppScreen::onStart(){
	lv_indev_wait_release(InputLVGL::getInstance()->getIndev());
	queue.reset();
	Events::listen(Facility::Input, &queue);
}

void AppScreen::onStop(){
	Events::unlisten(&queue);
}

void AppScreen::loop(){
	statusBar->loop();

	Event evt;
	while(queue.get(evt, 0)){
		auto data = (Input::Data*) evt.data;
		auto btn = data->btn;
		if(rotated && (btn == Input::Up || btn == Input::Down)){
			btn = btn == Input::Up ? Input::Down : Input::Up; // left-hand mode flips the screen
		}
		const bool press = data->action == Input::Data::Press;
		free(evt.data);

		if(btn == Input::Alt){
			if(press && !onAlt()){
				backToMenu();
				return;
			}
			continue;
		}

		auto& h = held[btn];
		if(press){
			h = { .down = true, .longFired = false, .since = millis(), .lastRepeat = 0 };
			onPress(btn);
		}else if(h.down){
			h.down = false;
			onRelease(btn, (uint32_t) (millis() - h.since));
		}
	}

	const uint64_t now = millis();
	for(int b = 0; b < 4; b++){
		auto& h = held[b];
		if(!h.down) continue;
		if(!h.longFired && now - h.since >= LongPressMs){
			h.longFired = true;
			h.lastRepeat = now;
			onLongPress((Input::Button) b);
		}
		if(h.longFired && (b == Input::Up || b == Input::Down) && now - h.lastRepeat >= RepeatMs){
			h.lastRepeat = now;
			onRepeat((Input::Button) b);
		}
	}

	if(now - lastTick >= TickMs){
		lastTick = now;
		tick();
	}
}
