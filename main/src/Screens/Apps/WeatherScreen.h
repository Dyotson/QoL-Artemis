#ifndef ARTEMIS_FIRMWARE_WEATHERSCREEN_H
#define ARTEMIS_FIRMWARE_WEATHERSCREEN_H

#include "AppScreen.h"
#include <ctime>

class WeatherScreen : public AppScreen {
public:
	WeatherScreen();

private:
	lv_obj_t* icon;
	lv_obj_t* temp;
	lv_obj_t* unitLabel;
	lv_obj_t* cond;
	lv_obj_t* hilo;
	lv_obj_t* age;
	lv_obj_t* sensor;
	lv_obj_t* help;

	time_t shownUpdate = -1;
	uint64_t lastSensor = 0;

	void tick() override;
	void refresh();
};

#endif //ARTEMIS_FIRMWARE_WEATHERSCREEN_H
