#ifndef ARTEMIS_FIRMWARE_CALLSCREEN_H
#define ARTEMIS_FIRMWARE_CALLSCREEN_H

#include "Notifs/Notif.h"
#include "LV_Interface/LVScreen.h"
#include <atomic>
#include "LV_Interface/LVStyle.h"
#include "Util/Events.h"

class CallScreen : public LVScreen {
public:
	/** Whether a call screen currently exists (used to avoid opening a second one) */
	static bool isShown();
	CallScreen();
	~CallScreen() override;

private:
	LVStyle textStyle;
	LVStyle btnStyle;
	LVStyle btnStyleFocus;
	LVStyle btnStylePress;

	Notif notif;

	void onIgnore();
	void onReject();

	static constexpr uint32_t NotifInterval = 1000;
	uint32_t notifTime;
	void onStart() override;
	void onStop() override;

	static std::atomic<bool> shown;
	bool quiet = false; // quiet hours: no ringtone, no LEDs

	EventQueue evts;
	bool altPress = false;
	void loop() override;

	void buildUI();

};


#endif //ARTEMIS_FIRMWARE_CALLSCREEN_H
