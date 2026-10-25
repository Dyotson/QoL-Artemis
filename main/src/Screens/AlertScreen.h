#ifndef ARTEMIS_FIRMWARE_ALERTSCREEN_H
#define ARTEMIS_FIRMWARE_ALERTSCREEN_H

#include "LV_Interface/LVScreen.h"
#include "Util/Events.h"

/** Full-screen "alarm / timer ringing" screen. Select/Alt stop it, Up/Down snooze. */
class AlertScreen : public LVScreen {
public:
	AlertScreen();
	~AlertScreen() override;

private:
	EventQueue queue;
	lv_obj_t* title;
	lv_obj_t* big;
	uint64_t shownAt;

	void onStart() override;
	void onStop() override;
	void loop() override;
	void close();
};

#endif //ARTEMIS_FIRMWARE_ALERTSCREEN_H
