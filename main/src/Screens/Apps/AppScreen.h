#ifndef ARTEMIS_FIRMWARE_APPSCREEN_H
#define ARTEMIS_FIRMWARE_APPSCREEN_H

#include "LV_Interface/LVScreen.h"
#include "UIElements/StatusBar.h"
#include "Devices/Input.h"
#include "Settings/Settings.h"
#include "Util/Events.h"
#include <cstdint>

/**
 * Base for the QoL apps (stopwatch, timer, alarms, world clock, weather, system info):
 * themed background + status bar, a content area below it, theme colours/fonts, and
 * button handling with long-press and auto-repeat. Alt goes back to the main menu.
 */
class AppScreen : public LVScreen {
public:
	AppScreen(const char* title);
	~AppScreen() override;

protected:
	static constexpr lv_coord_t Top = 15;			// status bar height
	static constexpr lv_coord_t ContentH = 128 - Top;

	lv_obj_t* content;	// 128 x ContentH, below the status bar
	ThemeStruct theme;

	lv_color_t textColor() const;
	lv_color_t dimColor() const;		// for secondary text

	/** Label helper. font: e.g. &devin, &landerfont, &lv_font_unscii_8/16, &clockfont */
	lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, const char* text = "", lv_color_t color = lv_color_black(), bool useColor = false);

	// Button callbacks. Alt is handled by the base (back to menu) unless onAlt() returns true.
	virtual void onPress(Input::Button btn){}
	virtual void onRelease(Input::Button btn, uint32_t heldMs){}
	/** Fired once when a button is held for LongPressMs */
	virtual void onLongPress(Input::Button btn){}
	/** Fired repeatedly while Up/Down are held, after the long-press delay (for fast adjust) */
	virtual void onRepeat(Input::Button btn){}
	virtual bool onAlt(){ return false; }

	/** Called every loop, at most every TickMs */
	virtual void tick(){}

	static constexpr uint32_t LongPressMs = 600;
	static constexpr uint32_t RepeatMs = 120;
	static constexpr uint32_t TickMs = 50;

	void backToMenu();

	void loop() override;
	void onStart() override;
	void onStop() override;

private:
	lv_obj_t* bg;
	StatusBar* statusBar;
	EventQueue queue;

	struct Held {
		bool down = false;
		bool longFired = false;
		uint64_t since = 0;
		uint64_t lastRepeat = 0;
	} held[4];

	uint64_t lastTick = 0;
	bool rotated = false;
};


#endif //ARTEMIS_FIRMWARE_APPSCREEN_H
