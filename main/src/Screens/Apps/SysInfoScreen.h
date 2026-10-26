#ifndef ARTEMIS_FIRMWARE_SYSINFOSCREEN_H
#define ARTEMIS_FIRMWARE_SYSINFOSCREEN_H

#include "AppScreen.h"

class SysInfoScreen : public AppScreen {
public:
	SysInfoScreen();

private:
	lv_obj_t* list;
	uint64_t lastRefresh = 0;
	lv_coord_t scrollY = 0;
	void applyScroll();

	void onPress(Input::Button btn) override;
	void onRepeat(Input::Button btn) override;
	void tick() override;
	void refresh();
};

#endif //ARTEMIS_FIRMWARE_SYSINFOSCREEN_H
