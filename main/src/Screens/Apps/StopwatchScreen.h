#ifndef ARTEMIS_FIRMWARE_STOPWATCHSCREEN_H
#define ARTEMIS_FIRMWARE_STOPWATCHSCREEN_H

#include "AppScreen.h"

class Clocks;

class StopwatchScreen : public AppScreen {
public:
	StopwatchScreen();

private:
	Clocks& clocks;
	lv_obj_t* mainTime;
	lv_obj_t* hundredths;
	lv_obj_t* lapList;
	lv_obj_t* hint;
	size_t shownLaps = SIZE_MAX;
	bool shownRunning = true;

	void onPress(Input::Button btn) override;
	void tick() override;
	void refreshLaps();
};

#endif //ARTEMIS_FIRMWARE_STOPWATCHSCREEN_H
