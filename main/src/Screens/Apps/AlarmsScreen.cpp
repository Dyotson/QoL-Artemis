#include "AlarmsScreen.h"
#include "Services/Clocks.h"
#include "Theme/theme.h"
#include "Util/Services.h"
#include <cstdio>

static constexpr uint8_t DayPresets[] = { 0, 0x7f, 0b0111110, 0b1000001 };

AlarmsScreen::AlarmsScreen() : AppScreen("ALARMS"), clocks(*(Clocks*) Services.get(Service::Clocks)){
	h24 = ((Settings*) Services.get(Service::Settings))->get().timeFormat24h;

	for(int i = 0; i < ExtSettingsStruct::AlarmCount; i++){
		auto& r = rows[i];
		r.box = lv_obj_create(content);
		lv_obj_set_size(r.box, 120, 22);
		lv_obj_set_pos(r.box, 4, 14 + i * 24);
		lv_obj_set_style_border_color(r.box, textColor(), 0);
		lv_obj_set_style_border_width(r.box, 0, 0);
		lv_obj_clear_flag(r.box, LV_OBJ_FLAG_SCROLLABLE);

		r.time = label(r.box, &lv_font_unscii_16, "");
		lv_obj_align(r.time, LV_ALIGN_LEFT_MID, 3, 0);
		r.state = label(r.box, &landerfont, "");
		lv_obj_align(r.state, LV_ALIGN_TOP_RIGHT, -3, 3);
		r.days = label(r.box, &devin, "");
		lv_obj_align(r.days, LV_ALIGN_BOTTOM_RIGHT, -3, -3);
	}

	footer = label(content, &devin, "");
	lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -4);

	refresh();
}

const char* AlarmsScreen::daysText(uint8_t days){
	switch(days){
		case 0: return "ONCE";
		case 0x7f: return "DAILY";
		case 0b0111110: return "M-F";
		case 0b1000001: return "WKND";
		default: return "CUSTOM";
	}
}

void AlarmsScreen::refresh(){
	for(int i = 0; i < ExtSettingsStruct::AlarmCount; i++){
		auto& r = rows[i];
		const bool isEdit = edit != Edit::None && i == selected;
		const AlarmConfig a = isEdit ? editing : clocks.getAlarm(i);

		char buf[24];
		if(h24) snprintf(buf, sizeof(buf), "%02d:%02d", a.hour, a.minute);
		else snprintf(buf, sizeof(buf), "%2d:%02d%s", (a.hour + 11) % 12 + 1, a.minute, a.hour < 12 ? "a" : "p");

		// While editing, blink the field being changed
		if(isEdit && (lv_tick_get() / 300) % 2){
			if(edit == Edit::Hour){ buf[0] = buf[1] = ' '; }
			else if(edit == Edit::Minute){ buf[3] = buf[4] = ' '; }
		}
		lv_label_set_text(r.time, buf);
		lv_label_set_text(r.state, a.enabled || isEdit ? "ON" : "OFF");
		lv_label_set_text(r.days, isEdit && edit == Edit::Days && (lv_tick_get() / 300) % 2 ? "" : daysText(a.days));

		const lv_color_t c = (a.enabled || isEdit) ? textColor() : dimColor();
		lv_obj_set_style_text_color(r.time, c, 0);
		lv_obj_set_style_text_color(r.state, c, 0);
		lv_obj_set_style_text_color(r.days, c, 0);
		lv_obj_set_style_border_width(r.box, i == selected ? 1 : 0, 0);
	}

	if(edit != Edit::None){
		lv_label_set_text(footer, edit == Edit::Days ? "UP/DOWN repeat\nSEL save" : "UP/DOWN change\nSEL next");
		lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -4);
		return;
	}

	const int32_t next = clocks.minutesToNextAlarm();
	char buf[48];
	if(next < 0) snprintf(buf, sizeof(buf), "SEL on/off\nhold SEL to edit");
	else snprintf(buf, sizeof(buf), "next in %ldh %02ldm\nhold SEL to edit", next / 60, next % 60);
	lv_label_set_text(footer, buf);
	lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -4);
}

void AlarmsScreen::step(int dir){
	if(edit == Edit::None){
		selected = (selected + ExtSettingsStruct::AlarmCount + dir) % ExtSettingsStruct::AlarmCount;
	}else if(edit == Edit::Hour){
		editing.hour = (editing.hour + 24 + dir) % 24;
	}else if(edit == Edit::Minute){
		editing.minute = (editing.minute + 60 + dir) % 60;
	}else{
		int idx = 0;
		for(int i = 0; i < 4; i++) if(DayPresets[i] == editing.days) idx = i;
		editing.days = DayPresets[(idx + 4 + dir) % 4];
	}
	refresh();
}

void AlarmsScreen::onPress(Input::Button btn){
	// Screen order is top to bottom, so "up" goes to the previous row
	if(btn == Input::Up) step(edit == Edit::None ? -1 : 1);
	else if(btn == Input::Down) step(edit == Edit::None ? 1 : -1);
}

void AlarmsScreen::onRepeat(Input::Button btn){
	if(edit == Edit::Hour || edit == Edit::Minute){
		onPress(btn);
	}
}

void AlarmsScreen::onRelease(Input::Button btn, uint32_t heldMs){
	if(btn != Input::Select || heldMs >= LongPressMs) return;

	if(edit == Edit::None){
		auto a = clocks.getAlarm(selected);
		a.enabled = !a.enabled;
		clocks.setAlarm(selected, a);
	}else if(edit == Edit::Hour){
		edit = Edit::Minute;
	}else if(edit == Edit::Minute){
		edit = Edit::Days;
	}else{
		editing.enabled = true;
		clocks.setAlarm(selected, editing);
		edit = Edit::None;
	}
	refresh();
}

void AlarmsScreen::onLongPress(Input::Button btn){
	if(btn != Input::Select || edit != Edit::None) return;
	editing = clocks.getAlarm(selected);
	edit = Edit::Hour;
	refresh();
}

bool AlarmsScreen::onAlt(){
	if(edit == Edit::None) return false;
	edit = Edit::None; // cancel editing
	refresh();
	return true;
}

void AlarmsScreen::tick(){
	if(edit != Edit::None) refresh(); // blinking
}
