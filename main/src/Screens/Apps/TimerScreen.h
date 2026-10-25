#ifndef ARTEMIS_FIRMWARE_TIMERSCREEN_H
#define ARTEMIS_FIRMWARE_TIMERSCREEN_H

#include "AppScreen.h"

class Clocks;

class TimerScreen : public AppScreen {
public:
	TimerScreen();

private:
	Clocks& clocks;
	lv_obj_t* timeLabel;
	lv_obj_t* bar;
	lv_obj_t* hint;
	lv_obj_t* state;

	void onPress(Input::Button btn) override;
	void onRelease(Input::Button btn, uint32_t heldMs) override;
	void onLongPress(Input::Button btn) override;
	void onRepeat(Input::Button btn) override;
	void tick() override;
	void adjust(int dir);
};

#endif //ARTEMIS_FIRMWARE_TIMERSCREEN_H
