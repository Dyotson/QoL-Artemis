#include "SysInfoScreen.h"
#include "Devices/Battery.h"
#include "Notifs/Phone.h"
#include "Services/Diagnostics.h"
#include "Services/Time.h"
#include "Theme/theme.h"
#include "Util/Services.h"
#include "Util/stdafx.h"
#include <esp_app_desc.h>
#include <esp_heap_caps.h>
#include <cstdio>
#include <string>

SysInfoScreen::SysInfoScreen() : AppScreen("SYSTEM INFO"){
	list = lv_obj_create(content);
	lv_obj_set_size(list, 122, ContentH - 18);
	lv_obj_set_pos(list, 4, 15);
	lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_pad_row(list, 2, 0);
	lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
	refresh();
}

static std::string ago(uint64_t ms){
	char buf[24];
	const uint64_t s = (millis() - ms) / 1000;
	if(s < 60) snprintf(buf, sizeof(buf), "%llus ago", s);
	else if(s < 3600) snprintf(buf, sizeof(buf), "%llum ago", s / 60);
	else snprintf(buf, sizeof(buf), "%lluh%02llum ago", s / 3600, (s / 60) % 60);
	return buf;
}

void SysInfoScreen::refresh(){
	lastRefresh = millis();
	const lv_coord_t scroll = lv_obj_get_scroll_y(list);
	lv_obj_clean(list);

	auto line = [this](const char* key, const std::string& value){
		auto row = lv_obj_create(list);
		lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
		auto k = label(row, &devin, key, dimColor(), true);
		lv_obj_align(k, LV_ALIGN_TOP_LEFT, 0, 0);
		auto v = label(row, &devin, value.c_str());
		lv_label_set_long_mode(v, LV_LABEL_LONG_WRAP);
		lv_obj_set_width(v, 86);
		lv_obj_align(v, LV_ALIGN_TOP_LEFT, 34, 0);
	};

	char buf[64];
	auto app = esp_app_get_description();
	line("FW", app->version);

	const uint64_t up = millis() / 1000;
	snprintf(buf, sizeof(buf), "%llud %02lluh %02llum", up / 86400, (up / 3600) % 24, (up / 60) % 60);
	line("UP", buf);

	if(auto bat = (Battery*) Services.get(Service::Battery)){
		const auto chg = bat->getChargingState();
		snprintf(buf, sizeof(buf), "%d%%%s", bat->getPerc(), chg == Battery::ChargingState::Charging ? " charging" : (chg == Battery::ChargingState::Full ? " full" : ""));
		line("BATT", buf);
	}

	auto phone = (Phone*) Services.get(Service::Phone);
	auto diag = (Diagnostics*) Services.get(Service::Diag);
	if(phone && diag){
		const auto st = diag->getBleStats();
		line("PHONE", phone->isConnected() ? "iPhone connected" : (st.connected ? "linking..." : "not connected"));
		if(st.connected && st.interval){
			snprintf(buf, sizeof(buf), "%.0fms x%d", st.interval * 1.25f, st.latency + 1);
			line("LINK", buf);
		}
		snprintf(buf, sizeof(buf), "%lu", st.disconnects);
		line("DROPS", st.disconnects ? std::string(buf) + ", last " + ago(st.lastDisconnectMs) + " (" + Diagnostics::bleReasonText(st.lastReason) + ")" : "none since boot");
	}

	if(auto time = (Time*) Services.get(Service::Time)){
		const int16_t off = time->getUtcOffset();
		snprintf(buf, sizeof(buf), "UTC%+d:%02d %s", off / 60, abs(off % 60), time->utcOffsetKnown() ? "(phone)" : "(saved)");
		line("TZ", buf);
	}

	if(diag){
		const auto& log = diag->getBootLog();
		if(log.count){
			snprintf(buf, sizeof(buf), "%s", Diagnostics::reasonText(log.boots[0].reason));
			line("BOOT", buf);
		}
		int crashes = 0;
		for(int i = 0; i < log.count; i++) if(Diagnostics::reasonIsCrash(log.boots[i].reason)) crashes++;
		snprintf(buf, sizeof(buf), "%d in last %d boots", crashes, log.count);
		line("CRASH", buf);
		if(diag->hasCoredump()) line("DUMP", diag->coredumpSummary());
	}

	snprintf(buf, sizeof(buf), "%uk ram, %uk psram", heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024, heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
	line("FREE", buf);

	lv_obj_update_layout(list);
	lv_obj_scroll_to_y(list, scroll, LV_ANIM_OFF);
}

void SysInfoScreen::onPress(Input::Button btn){
	if(btn == Input::Up) lv_obj_scroll_by_bounded(list, 0, 20, LV_ANIM_ON);
	else if(btn == Input::Down) lv_obj_scroll_by_bounded(list, 0, -20, LV_ANIM_ON);
}

void SysInfoScreen::onRepeat(Input::Button btn){
	onPress(btn);
}

void SysInfoScreen::tick(){
	if(millis() - lastRefresh > 2000) refresh();
}
