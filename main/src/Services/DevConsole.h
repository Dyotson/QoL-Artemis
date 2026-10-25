#ifndef ARTEMIS_FIRMWARE_DEVCONSOLE_H
#define ARTEMIS_FIRMWARE_DEVCONSOLE_H

#include "Util/Threaded.h"
#include <esp_pm.h>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

class LVGL;

/**
 * Line-based command console on the USB Serial/JTAG port (the same port that carries
 * the logs). Used to inspect and drive the watch from a computer: screenshots, button
 * injection, diagnostics, triggering weather updates, etc. Type "help" for the list.
 *
 * Also the periodic housekeeping tick for Diagnostics.
 *
 * Light sleep would drop the USB link, so once a host has sent something while USB
 * power is present, light sleep is blocked until the cable is unplugged.
 */
class DevConsole : public Threaded {
public:
	DevConsole(LVGL& lvgl);

	using Args = std::vector<std::string>;
	using Handler = std::function<void(const Args& args)>;

	static void addCommand(const char* name, const char* help, Handler handler);

private:
	LVGL& lvgl;

	struct Command {
		std::string help;
		Handler handler;
	};
	static std::map<std::string, Command>& commands();
	static std::mutex cmdMut;

	std::string line;
	esp_pm_lock_handle_t pmLock = nullptr;
	bool pmLocked = false;
	uint64_t lastTick = 0;

	void loop() override;
	void execute(const std::string& line);
	void registerBuiltins();
	void updatePmLock(bool usbPresent);

	void screenshot();
};


#endif //ARTEMIS_FIRMWARE_DEVCONSOLE_H
