#include <freertos/FreeRTOS.h>
#include <driver/gpio.h>
#include <nvs_flash.h>
#include <bootloader_random.h>
#include <esp_random.h>
#include <Devices/BatteryV2.h>
#include <Devices/BatteryV3.h>

#include "Settings/Settings.h"
#include "Pins.hpp"
#include "Periph/I2C.h"
#include "Periph/PinOut.h"
#include "Periph/Bluetooth.h"
#include "Devices/Battery.h"
#include "Devices/Display.h"
#include "Devices/Input.h"
#include "Devices/IMU.h"
#include "BLE/GAP.h"
#include "BLE/Client.h"
#include "BLE/Server.h"
#include "Notifs/Phone.h"
#include "LV_Interface/LVGL.h"
#include "LV_Interface/FSLVGL.h"
#include "LV_Interface/InputLVGL.h"
#include <lvgl/lvgl.h>
#include "Util/EfuseMeta.h"
#include "Theme/theme.h"
#include "Util/Services.h"
#include "Services/BacklightBrightness.h"
#include "Services/ChirpSystem.h"
#include "Services/Time.h"
#include "Services/StatusCenter.h"
#include "Services/SleepMan.h"
#include "Screens/ShutdownScreen.h"
#include "Screens/Lock/LockScreen.h"
#include "JigHWTest/JigHWTest.h"
#include "Util/Notes.h"
#include "Screens/Intro/IntroScreen.h"
#include "Filepaths.hpp"
#include "Screens/Lander/LunarLander.h"
#include "Screens/PerseCtrl/WiFiSTA.h"
#include "Screens/PerseCtrl/TCPClient.h"
#include "Services/Diagnostics.h"
#include "Services/DevConsole.h"
#include "BLE/ConMan.h"
#include "Settings/ExtSettings.h"
#include "Services/MelodyPlayer.h"
#include "Services/Clocks.h"
#include "Services/Weather.h"

LVGL* lvgl;
BacklightBrightness* bl;
SleepMan* sleepMan;

void shutdown(){
	lvgl->startScreen([](){ return std::make_unique<ShutdownScreen>(); });

	if(!lvgl->running()){
		lvgl->start();
	}

	lv_timer_handler();
	sleepMan->wake(true);
	if(!bl->isOn()){
		bl->fadeIn();
	}
	vTaskDelay(SleepMan::ShutdownTime-1000);
	sleepMan->shutdown();
}

//This works since it's same across all revisions.
//Change if necessary (since setLEDs() call could mess up another hardware revision)
static const gpio_num_t LEDs[] = { GPIO_NUM_17, GPIO_NUM_18, GPIO_NUM_43, GPIO_NUM_44, GPIO_NUM_45, GPIO_NUM_46 };

void setLEDs(){
	for(int i = 0; i < sizeof(LEDs) / sizeof(LEDs[0]); i++){
		const gpio_num_t pin = LEDs[i];
		gpio_config_t config = {
				.pin_bit_mask = 1ULL << pin,
				.mode = GPIO_MODE_OUTPUT,
				.pull_up_en = GPIO_PULLUP_DISABLE,
				.pull_down_en = GPIO_PULLDOWN_DISABLE,
				.intr_type = GPIO_INTR_DISABLE
		};

		gpio_config(&config);
		gpio_set_level(pin, 0);
	}
}

void init(){
	setLEDs();
	gpio_install_isr_service(ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM);

	auto ret = nvs_flash_init();
	if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	auto diag = new Diagnostics();
	Services.set(Service::Diag, diag);

	auto settings = new Settings();
	Services.set(Service::Settings, settings);

	auto extSettings = new ExtSettings();
	Services.set(Service::ExtSettings, extSettings);

	if(JigHWTest::checkJig()){
		printf("Jig\n");
		Pins::setLatest();
		auto test = new JigHWTest();
		test->start();
		vTaskDelete(nullptr);
	}else{
		printf("Hello\n");
	}

	if(!EfuseMeta::check()){
		while(true){
			vTaskDelay(1000);
			EfuseMeta::log();
		}
	}

	uint8_t rev = 0;
	if(!EfuseMeta::readRev(rev)){
		while(true){
			vTaskDelay(1000);
			printf("Failed to read hardware revision.");
		}
	}

	auto adc1 = new ADC(ADC_UNIT_1);

	auto blPwm = new PWM(Pins::get(Pin::LedBl), LEDC_CHANNEL_1, true);
	blPwm->detach();
	bl = new BacklightBrightness(blPwm);
	Services.set(Service::Backlight, bl);

	auto buzzPwm = new PWM(Pins::get(Pin::Buzz), LEDC_CHANNEL_0);
	auto audio = new ChirpSystem(*buzzPwm);
	Services.set(Service::Audio, audio);
	Services.set(Service::Melody, new MelodyPlayer(*audio));

	auto i2c = new I2C(I2C_NUM_0, (gpio_num_t) Pins::get(Pin::I2cSda), (gpio_num_t) Pins::get(Pin::I2cScl));
	auto imu = new IMU(*i2c);
	Services.set(Service::IMU, imu);

	auto disp = new Display(rev);
	auto input = new Input();
	Services.set(Service::Input, input);

	lvgl = new LVGL(*disp);
	Services.set(Service::LVGL, lvgl);
	auto theme = theme_init(lvgl->disp());
	lv_disp_set_theme(lvgl->disp(), theme);

	auto lvglInput = new InputLVGL();
	auto fs = new FSLVGL('S');

	sleepMan = new SleepMan(*lvgl);
	Services.set(Service::Sleep, sleepMan);

	auto status = new StatusCenter();
	Services.set(Service::Status, status);

	Battery* battery = nullptr;

	if(rev == 0 || rev == 1){
		battery = new BatteryV2(*adc1);
	}else{
		battery = new BatteryV3(*adc1);
	}

	if(battery->isShutdown()) return; // Stop initialization if battery is critical
	Services.set(Service::Battery, battery);

	auto rtc = new RTC(*i2c);
	auto time = new Time(*rtc);
	Services.set(Service::Time, time); // Time service is required as soon as Phone is up

	Services.set(Service::Weather, new Weather());

	auto bt = new Bluetooth();
	auto gap = new BLE::GAP();
	auto client = new BLE::Client(gap);
	auto server = new BLE::Server(gap);
	auto phone = new Phone(server, client);
	server->start();
	Services.set(Service::Phone, phone);
	client->setRecoveryAllowed([phone](){ return phone->getPhoneType() != Phone::PhoneType::Android; });

	DevConsole::addCommand("theme", "theme <1-9>  - preview a theme on the lock screen (not saved)", [](const DevConsole::Args& args){
		if(args.size() < 2) return;
		const int t = atoi(args[1].c_str());
		if(t < 1 || t > 9) return;
		lvgl->runOnUI([t](){
			auto settings = (Settings*) Services.get(Service::Settings);
			auto s = settings->get();
			s.themeData = createTheme((Theme) (t - 1));
			settings->set(s);
			FSLVGL::unloadCache();
			FSLVGL::loadCache(s.themeData.theme);
			lvgl->startScreen([](){ return std::make_unique<LockScreen>(); });
		});
		vTaskDelay(pdMS_TO_TICKS(1500));
	});

	DevConsole::addCommand("phone", "phone link / notification service state", [phone, client](const DevConsole::Args&){
		const auto st = client->getStatus();
		const char* types[] = { "none", "Android", "iPhone" };
		printf("link %d, gatt open %d, encrypted %d, discovery %d, services found %d, set up %d\n",
			   ConMan.isConnected(), st.opened, st.paired, st.discovering, st.established, st.populated);
		printf("phone type: %s, notifications: %lu\n", types[(int) phone->getPhoneType()], phone->getNotifsCount());
	});

	srand(esp_random());

	new DevConsole(*lvgl);

	Services.set(Service::Clocks, new Clocks());

	FSLVGL::loadCache(settings->get().themeData.theme);

	// Load start screen here
	lvgl->startScreen([](){ return std::make_unique<IntroScreen>(); });

	// Start UI thread after initialization
	lvgl->start();

	bl->fadeIn();

	if(settings->get().notificationSounds){
		audio->play({
							Chirp{ .startFreq = 250, .endFreq = 250, .duration = 100 },
							Chirp{ .startFreq = 0, .endFreq = 0, .duration = 100 },
							Chirp{ .startFreq = 250, .endFreq = 250, .duration = 100 },
							Chirp{ .startFreq = 0, .endFreq = 0, .duration = 250 },
							Chirp{ .startFreq = 500, .endFreq = 500, .duration = 200 }
					});

		vTaskDelay(2500);
	}

	// Start Battery scanning after everything else, otherwise Critical
	// Battery event might come while initialization is still in progress
	battery->begin();
}

extern "C" void app_main(void){
	init();

	vTaskDelete(nullptr);
}
