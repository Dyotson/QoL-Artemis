#ifndef ARTEMIS_FIRMWARE_ALARMSSCREEN_H
#define ARTEMIS_FIRMWARE_ALARMSSCREEN_H

#include "AppScreen.h"
#include "Settings/ExtSettings.h"

class Clocks;

class AlarmsScreen : public AppScreen {
public:
	AlarmsScreen();

private:
	Clocks& clocks;
	bool h24;

	struct Row {
		lv_obj_t* box;
		lv_obj_t* time;
		lv_obj_t* days;
		lv_obj_t* state;
	} rows[ExtSettingsStruct::AlarmCount];
	lv_obj_t* footer;

	uint8_t selected = 0;

	enum class Edit { None, Hour, Minute, Days } edit = Edit::None;
	AlarmConfig editing;

	void onPress(Input::Button btn) override;
	void onRelease(Input::Button btn, uint32_t heldMs) override;
	void onLongPress(Input::Button btn) override;
	void onRepeat(Input::Button btn) override;
	bool onAlt() override;
	void tick() override;

	void step(int dir);
	void refresh();
	static const char* daysText(uint8_t days);
};

#endif //ARTEMIS_FIRMWARE_ALARMSSCREEN_H
