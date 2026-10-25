#ifndef ARTEMIS_FIRMWARE_WEATHERELEMENT_H
#define ARTEMIS_FIRMWARE_WEATHERELEMENT_H

#include "LV_Interface/LVObject.h"
#include <ctime>

/** Small "icon + 18°" weather readout for the watch faces. Hidden while there's no fresh data. */
class WeatherElement : public LVObject {
public:
	WeatherElement(lv_obj_t* parent, lv_color_t color);

	void loop();
	void refresh();

	/** Font with the degree sign (landerfont + fallback) */
	static const lv_font_t* font();

private:
	lv_obj_t* icon;
	lv_obj_t* temp;
	time_t shownUpdate = -1;
	uint64_t lastCheck = 0;
};

#endif //ARTEMIS_FIRMWARE_WEATHERELEMENT_H
