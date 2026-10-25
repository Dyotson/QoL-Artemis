#include "TimerScreen.h"
#include "Services/Clocks.h"
#include "Theme/theme.h"
#include "Util/Services.h"
#include <cstdio>

TimerScreen::TimerScreen() : AppScreen("TIMER"), clocks(*(Clocks*) Services.get(Service::Clocks)){
	timeLabel = label(content, &clockfont, "05:00");
	lv_obj_align(timeLabel, LV_ALIGN_TOP_MID, 0, 18);

	bar = lv_bar_create(content);
	lv_obj_set_size(bar, 100, 6);
	lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 52);
	lv_bar_set_range(bar, 0, 1000);
	lv_obj_set_style_bg_color(bar, dimColor(), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(bar, LV_OPA_40, LV_PART_MAIN);
	lv_obj_set_style_bg_color(bar, textColor(), LV_PART_INDICATOR);
	lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
	lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
	lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);

	state = label(content, &landerfont, "");
	lv_obj_align(state, LV_ALIGN_TOP_MID, 0, 64);

	hint = label(content, &devin, "");
	lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -3);

	tick();
}

void TimerScreen::adjust(int dir){
	if(clocks.timerRunning()) return;
	const uint32_t cur = clocks.timerPreset();
	// 15 s steps below 5 minutes, 1 minute steps above
	const int32_t step = (cur < 300 || (cur == 300 && dir < 0)) ? 15 : 60;
	int32_t next = (int32_t) cur + dir * step;
	if(next < 15) next = 15;
	clocks.timerSet(next);
	tick();
}

void TimerScreen::onPress(Input::Button btn){
	if(btn == Input::Up) adjust(1);
	else if(btn == Input::Down) adjust(-1);
}

void TimerScreen::onRelease(Input::Button btn, uint32_t heldMs){
	if(btn == Input::Select && heldMs < LongPressMs){
		clocks.timerStartPause();
		tick();
	}
}

void TimerScreen::onLongPress(Input::Button btn){
	if(btn == Input::Select){
		clocks.timerReset();
		tick();
	}
}

void TimerScreen::onRepeat(Input::Button btn){
	if(btn == Input::Up) adjust(1);
	else if(btn == Input::Down) adjust(-1);
}

void TimerScreen::tick(){
	const uint32_t ms = clocks.timerRemainingMs();
	const uint32_t secs = (ms + 999) / 1000; // count down like a kitchen timer: 00:01 until it's done
	char buf[24];
	if(secs >= 3600) snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu", secs / 3600, (secs / 60) % 60, secs % 60);
	else snprintf(buf, sizeof(buf), "%02lu:%02lu", secs / 60, secs % 60);
	lv_label_set_text(timeLabel, buf);

	const bool running = clocks.timerRunning();
	const bool paused = clocks.timerPaused();
	const uint32_t preset = clocks.timerPreset();
	lv_bar_set_value(bar, running ? (int32_t) ((uint64_t) ms * 1000 / ((uint64_t) preset * 1000)) : 1000, LV_ANIM_OFF);

	lv_label_set_text(state, !running ? "SET TIME" : (paused ? "PAUSED" : "RUNNING"));
	lv_label_set_text(hint, !running ? "UP/DOWN set time\nSEL start" : "SEL pause/resume\nhold SEL reset");
	lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);
}
