#include "StopwatchScreen.h"
#include "Services/Clocks.h"
#include "Theme/theme.h"
#include "Util/Services.h"
#include <cstdio>

static void fmtTime(char* buf, size_t len, uint64_t ms, bool withHundredths){
	const uint32_t h = ms / 3600000, m = (ms / 60000) % 60, s = (ms / 1000) % 60, cs = (ms / 10) % 100;
	if(h > 0) snprintf(buf, len, "%lu:%02lu:%02lu", h, m, s);
	else if(withHundredths) snprintf(buf, len, "%lu:%02lu.%02lu", m, s, cs);
	else snprintf(buf, len, "%02lu:%02lu", m, s);
}

StopwatchScreen::StopwatchScreen() : AppScreen("STOPWATCH"), clocks(*(Clocks*) Services.get(Service::Clocks)){
	mainTime = label(content, &clockfont, "00:00");
	lv_obj_align(mainTime, LV_ALIGN_TOP_MID, 0, 14);

	hundredths = label(content, &landerfont, ".00");
	lv_obj_align(hundredths, LV_ALIGN_TOP_MID, 0, 42);

	lapList = lv_obj_create(content);
	lv_obj_set_size(lapList, 120, 40);
	lv_obj_align(lapList, LV_ALIGN_TOP_MID, 0, 54);
	lv_obj_set_flex_flow(lapList, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_pad_row(lapList, 1, 0);
	lv_obj_clear_flag(lapList, LV_OBJ_FLAG_SCROLLABLE);

	hint = label(content, &devin, "");
	lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);

	tick();
}

void StopwatchScreen::onPress(Input::Button btn){
	if(btn == Input::Select){
		clocks.swStartStop();
	}else if(btn == Input::Up){
		if(clocks.swRunning()) clocks.swLap();
	}else if(btn == Input::Down){
		if(!clocks.swRunning()) clocks.swReset();
	}
	tick();
}

void StopwatchScreen::tick(){
	const uint64_t ms = clocks.swElapsedMs();
	char buf[24];

	if(ms >= 3600000){
		fmtTime(buf, sizeof(buf), ms, false);
		lv_label_set_text(mainTime, buf);
		lv_label_set_text(hundredths, "");
	}else{
		snprintf(buf, sizeof(buf), "%02lu:%02lu", (uint32_t) (ms / 60000) % 60, (uint32_t) (ms / 1000) % 60);
		lv_label_set_text(mainTime, buf);
		snprintf(buf, sizeof(buf), ".%02lu", (uint32_t) (ms / 10) % 100);
		lv_label_set_text(hundredths, buf);
	}

	const bool running = clocks.swRunning();
	if(running != shownRunning){
		shownRunning = running;
		lv_label_set_text(hint, running ? "SEL stop   UP lap" : (ms ? "SEL go   DOWN reset" : "SEL start"));
		lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);
	}

	refreshLaps();
}

void StopwatchScreen::refreshLaps(){
	const auto laps = clocks.swLaps();
	if(laps.size() == shownLaps) return;
	shownLaps = laps.size();

	lv_obj_clean(lapList);
	// Newest first, with the split time of each lap
	for(int i = (int) laps.size() - 1, shown = 0; i >= 0 && shown < 5; i--, shown++){
		const uint64_t split = laps[i] - (i > 0 ? laps[i - 1] : 0);
		char total[24], lap[24], line[64];
		fmtTime(total, sizeof(total), laps[i], true);
		fmtTime(lap, sizeof(lap), split, true);
		snprintf(line, sizeof(line), "#%-2d %9s %10s", i + 1, lap, total);
		auto l = label(lapList, &devin, line, shown == 0 ? textColor() : dimColor(), true);
		(void) l;
	}
}
