#ifndef ARTEMIS_FIRMWARE_WORLDCLOCKSCREEN_H
#define ARTEMIS_FIRMWARE_WORLDCLOCKSCREEN_H

#include "AppScreen.h"
#include "Settings/ExtSettings.h"

class WorldClockScreen : public AppScreen {
public:
	WorldClockScreen();

private:
	bool h24;
	lv_obj_t* homeTime;
	lv_obj_t* homeInfo;

	struct Row {
		lv_obj_t* box;
		lv_obj_t* name;
		lv_obj_t* time;
		lv_obj_t* diff;
	} rows[ExtSettingsStruct::WorldClocks];
	lv_obj_t* footer;

	uint8_t cities[ExtSettingsStruct::WorldClocks];
	uint8_t selected = 0;
	bool editing = false;

	void onPress(Input::Button btn) override;
	void onRelease(Input::Button btn, uint32_t heldMs) override;
	void onLongPress(Input::Button btn) override;
	void onRepeat(Input::Button btn) override;
	bool onAlt() override;
	void tick() override;

	void refresh();
	void cycleCity(int dir);
	void save();
	void fmt(char* buf, size_t len, const tm& t) const;
};

#endif //ARTEMIS_FIRMWARE_WORLDCLOCKSCREEN_H
