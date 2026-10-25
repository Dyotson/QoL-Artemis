#include "DevConsole.h"
#include "Diagnostics.h"
#include "SleepMan.h"
#include "Devices/Input.h"
#include "LV_Interface/LVGL.h"
#include "Notifs/Phone.h"
#include "Pins.hpp"
#include "Util/Events.h"
#include "Util/Services.h"
#include "Util/stdafx.h"
#include <driver/gpio.h>
#include <esp_app_desc.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_system.h>
#include <extra/others/snapshot/lv_snapshot.h>
#include <freertos/task.h>
#include <mbedtls/base64.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <memory>

static const char* TAG = "Console";

std::mutex DevConsole::cmdMut;

std::map<std::string, DevConsole::Command>& DevConsole::commands(){
	static std::map<std::string, Command> cmds;
	return cmds;
}

void DevConsole::addCommand(const char* name, const char* help, Handler handler){
	std::lock_guard lock(cmdMut);
	commands()[name] = Command{ help, std::move(handler) };
}

DevConsole::DevConsole(LVGL& lvgl) : Threaded("Console", 4 * 1024, 3, 0), lvgl(lvgl){
	esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "console", &pmLock);
	registerBuiltins();
	start();
}

void DevConsole::updatePmLock(bool usbPresent){
	if(pmLock == nullptr) return;

	if(!usbPresent && pmLocked){
		esp_pm_lock_release(pmLock);
		pmLocked = false;
	}
}

void DevConsole::loop(){
	const bool usb = gpio_get_level((gpio_num_t) Pins::get(Pin::Usb)) == 1;
	updatePmLock(usb);

	if(millis() - lastTick >= 10000){
		lastTick = millis();
		if(auto diag = (Diagnostics*) Services.get(Service::Diag)){
			diag->tick();
		}
	}

	if(!usb){
		vTaskDelay(pdMS_TO_TICKS(2000));
		return;
	}

	char buf[64];
	const int n = read(STDIN_FILENO, buf, sizeof(buf));
	if(n <= 0){
		vTaskDelay(pdMS_TO_TICKS(30));
		return;
	}

	if(!pmLocked && pmLock){
		esp_pm_lock_acquire(pmLock);
		pmLocked = true;
	}

	for(int i = 0; i < n; i++){
		const char c = buf[i];
		if(c == '\n' || c == '\r'){
			if(!line.empty()){
				execute(line);
				line.clear();
			}
		}else if(line.size() < 256){
			line.push_back(c);
		}
	}
}

void DevConsole::execute(const std::string& cmdLine){
	Args args;
	std::istringstream ss(cmdLine);
	std::string tok;
	while(ss >> tok){
		args.push_back(tok);
	}
	if(args.empty()) return;

	Handler handler;
	{
		std::lock_guard lock(cmdMut);
		auto it = commands().find(args[0]);
		if(it == commands().end()){
			printf("ERR unknown command '%s', try 'help'\n", args[0].c_str());
			return;
		}
		handler = it->second.handler;
	}

	handler(args);
	printf("OK\n");
	fflush(stdout);
}

static bool parseButton(const std::string& s, Input::Button& btn){
	if(s == "u" || s == "up") btn = Input::Up;
	else if(s == "d" || s == "down") btn = Input::Down;
	else if(s == "s" || s == "select") btn = Input::Select;
	else if(s == "a" || s == "alt") btn = Input::Alt;
	else return false;
	return true;
}

void DevConsole::registerBuiltins(){
	addCommand("help", "list commands", [](const Args&){
		std::lock_guard lock(cmdMut);
		for(const auto& [name, cmd] : commands()){
			printf("  %-10s %s\n", name.c_str(), cmd.help.c_str());
		}
	});

	addCommand("info", "firmware, uptime, heap and task stacks", [](const Args&){
		auto app = esp_app_get_description();
		printf("fw %s (%s %s), uptime %llus\n", app->version, app->date, app->time, millis() / 1000);
		printf("heap internal free %u (min %u, largest %u), psram free %u\n",
			   heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
			   heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

		const UBaseType_t count = uxTaskGetNumberOfTasks();
		auto tasks = (TaskStatus_t*) malloc(sizeof(TaskStatus_t) * (count + 4));
		if(tasks == nullptr) return;
		const UBaseType_t got = uxTaskGetSystemState(tasks, count + 4, nullptr);
		printf("%-16s prio minfree\n", "task");
		for(UBaseType_t i = 0; i < got; i++){
			printf("%-16s %4u %7lu\n", tasks[i].pcTaskName, tasks[i].uxCurrentPriority, (unsigned long) tasks[i].usStackHighWaterMark);
		}
		free(tasks);
	});

	addCommand("btn", "btn <u|d|s|a> [hold_ms]  - press a button", [](const Args& args){
		Input::Button btn;
		if(args.size() < 2 || !parseButton(args[1], btn)){
			printf("ERR usage: btn <u|d|s|a> [hold_ms]\n");
			return;
		}
		const int hold = args.size() > 2 ? atoi(args[2].c_str()) : 60;
		Events::post(Facility::Input, Input::Data{ .btn = btn, .action = Input::Data::Press });
		vTaskDelay(pdMS_TO_TICKS(hold));
		Events::post(Facility::Input, Input::Data{ .btn = btn, .action = Input::Data::Release });
		vTaskDelay(pdMS_TO_TICKS(150));
	});

	addCommand("wake", "wake the screen", [](const Args&){
		if(auto sleep = (SleepMan*) Services.get(Service::Sleep)){
			sleep->wake();
			vTaskDelay(pdMS_TO_TICKS(400));
		}
	});

	addCommand("sleep", "turn the screen off (sleep)", [](const Args&){
		if(auto sleep = (SleepMan*) Services.get(Service::Sleep)){
			sleep->requestSleep();
		}
	});

	addCommand("shot", "screenshot as base64 RGB565 (128x128)", [this](const Args&){
		screenshot();
	});

	addCommand("reboot", "restart the watch", [](const Args&){
		fflush(stdout);
		vTaskDelay(pdMS_TO_TICKS(100));
		esp_restart();
	});

	addCommand("diag", "reset history, BLE link stats, stored crash", [](const Args& args){
		auto diag = (Diagnostics*) Services.get(Service::Diag);
		if(!diag) return;

		if(args.size() > 1 && args[1] == "clear"){
			diag->clearCoredump();
			printf("coredump cleared\n");
			return;
		}

		const auto& log = diag->getBootLog();
		printf("boots: %lu\n", log.bootCount);
		for(int i = 0; i < log.count; i++){
			printf("  #%d %-22s prev uptime %lus\n", i, Diagnostics::reasonText(log.boots[i].reason), log.boots[i].prevUptime);
		}
		const auto ble = diag->getBleStats();
		printf("ble: %s, connects %lu, disconnects %lu, last reason 0x%02x (%s), %llus ago\n",
			   ble.connected ? "connected" : "disconnected", ble.connects, ble.disconnects, ble.lastReason,
			   Diagnostics::bleReasonText(ble.lastReason), ble.lastDisconnectMs ? (millis() - ble.lastDisconnectMs) / 1000 : 0);
		printf("link: interval %.2fms, latency %d, timeout %dms\n", ble.interval * 1.25f, ble.latency, ble.timeout * 10);
		printf("coredump: %s\n", diag->hasCoredump() ? diag->coredumpSummary().c_str() : "none");
	});

	addCommand("log", "log <tag|*> <0-5>  - set log level", [](const Args& args){
		if(args.size() < 3) return;
		esp_log_level_set(args[1].c_str(), (esp_log_level_t) atoi(args[2].c_str()));
	});

	addCommand("bootlog", "bootlog <0-5>  - log level applied from boot (persists)", [](const Args& args){
		auto diag = (Diagnostics*) Services.get(Service::Diag);
		if(!diag || args.size() < 2) return;
		diag->setBootLogLevel(atoi(args[1].c_str()));
	});

	addCommand("notif", "notif <title> <message...>  - inject a test notification", [](const Args& args){
		auto phone = (Phone*) Services.get(Service::Phone);
		if(!phone || args.size() < 2) return;
		std::string msg;
		for(size_t i = 2; i < args.size(); i++){
			if(!msg.empty()) msg += ' ';
			msg += args[i];
		}
		phone->debugInject(args[1], msg);
	});
}

void DevConsole::screenshot(){
	// Shared so a late-running closure (after our timeout) never touches a deleted semaphore
	struct Done {
		SemaphoreHandle_t sem = xSemaphoreCreateBinary();
		~Done(){ vSemaphoreDelete(sem); }
	};
	auto done = std::make_shared<Done>();

	lvgl.runOnUI([done](){
		lv_img_dsc_t* snap = lv_snapshot_take(lv_scr_act(), LV_IMG_CF_TRUE_COLOR);
		if(snap == nullptr){
			printf("ERR snapshot failed\n");
			xSemaphoreGive(done->sem);
			return;
		}

		// LVGL 8.3's lv_snapshot_take() leaves data_size unset
		const size_t len = (size_t) snap->header.w * snap->header.h * sizeof(lv_color_t);
		size_t outLen = 0;
		mbedtls_base64_encode(nullptr, 0, &outLen, snap->data, len);
		auto out = (unsigned char*) heap_caps_malloc(outLen + 1, MALLOC_CAP_SPIRAM);
		if(out && mbedtls_base64_encode(out, outLen + 1, &outLen, snap->data, len) == 0){
			printf("SHOT %d %d\n", (int) snap->header.w, (int) snap->header.h);
			for(size_t i = 0; i < outLen; i += 512){
				fwrite(out + i, 1, std::min<size_t>(512, outLen - i), stdout);
				fflush(stdout);
			}
			printf("\nENDSHOT\n");
		}
		free(out);
		lv_snapshot_free(snap);
		xSemaphoreGive(done->sem);
	});

	xSemaphoreTake(done->sem, pdMS_TO_TICKS(5000));
}
