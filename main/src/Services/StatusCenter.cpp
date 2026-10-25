#include "StatusCenter.h"
#include "Util/Services.h"
#include "SleepMan.h"
#include "Pins.hpp"
#include "Services/Time.h"
#include "Settings/ExtSettings.h"

StatusCenter::StatusCenter() : Threaded("Status", 2048), events(12),
chirp(*((ChirpSystem*) Services.get(Service::Audio))),
settings(*((Settings*) Services.get(Service::Settings)))
{
	Events::listen(Facility::Phone, &events);
	Events::listen(Facility::Battery, &events);

	auto pwmR = new PWM(Pins::get(Pin::Rgb_r), LEDC_CHANNEL_2, true);
	auto pwmG = new PWM(Pins::get(Pin::Rgb_g), LEDC_CHANNEL_3, true);
	auto pwmB = new PWM(Pins::get(Pin::Rgb_b), LEDC_CHANNEL_4, true);

	led = new RGBLEDController(*pwmR, *pwmG, *pwmB);
	led->begin();

	singleLeds[0] = std::make_shared<DigitalLEDController>((gpio_num_t) Pins::get(Pin::Led_1));
	singleLeds[1] = std::make_shared<DigitalLEDController>((gpio_num_t) Pins::get(Pin::Led_2));
	singleLeds[2] = std::make_shared<DigitalLEDController>((gpio_num_t) Pins::get(Pin::Led_3));
	singleLeds[3] = std::make_shared<DigitalLEDController>((gpio_num_t) Pins::get(Pin::Led_4));
	singleLeds[4] = std::make_shared<DigitalLEDController>((gpio_num_t) Pins::get(Pin::Led_5));
	singleLeds[5] = std::make_shared<DigitalLEDController>((gpio_num_t) Pins::get(Pin::Led_6));

	for(const std::shared_ptr<DigitalLEDController>& singleLed : singleLeds){
		singleLed->begin();
		singleLed->setSolid(0);
	}

	updateLED();

	esp_timer_create_args_t args = {
			.callback = [](void* arg){ static_cast<StatusCenter*>(arg)->onPhoneLost(); },
			.arg = this,
			.dispatch_method = ESP_TIMER_TASK,
			.name = "PhoneLost",
			.skip_unhandled_events = true
	};
	esp_timer_create(&args, &lostTimer);

	args.callback = [](void* arg){ static_cast<StatusCenter*>(arg)->alarmTick(); };
	args.name = "AlarmTick";
	esp_timer_create(&args, &alarmTimer);

	start();
}

StatusCenter::~StatusCenter(){
	Events::unlisten(&events);
}

void StatusCenter::loop(){
	Event evt;
	if(!events.get(evt, portMAX_DELAY)) return;

	if(evt.facility == Facility::Phone){
		auto data = (Phone::Event*) evt.data;
		processPhone(*data);
	}else if(evt.facility == Facility::Battery){
		auto data = (Battery::Event*) evt.data;
		processBatt(*data);
	}

	free(evt.data);
}

void StatusCenter::processPhone(const Phone::Event& evt){
	auto phone = (Phone*) Services.get(Service::Phone);
	hasNotifs = phone->getNotifsCount() != 0;

	if(evt.action == Phone::Event::Connected){
		esp_timer_stop(lostTimer);
	}else if(evt.action == Phone::Event::Disconnected){
		esp_timer_stop(lostTimer);
		esp_timer_start_once(lostTimer, LostAlertMs * 1000);
	}

	if((evt.action == Phone::Event::Added || evt.action == Phone::Event::Changed) && evt.alert){
		const auto notif = phone->getNotif(evt.data.addChgRem.id);
		const bool isCall = notif.category == Notif::Category::IncomingCall;
		if(isCall) return; // the call screen plays the ringtone
		if(inQuietHours()) return;

		if(settings.get().notificationSounds && !audioBlocked){
			beep();
		}

		if(settings.get().ledEnable){
			circularBlink();
		}
	}
}

bool StatusCenter::inQuietHours(){
	auto ext = (ExtSettings*) Services.get(Service::ExtSettings);
	auto time = (Time*) Services.get(Service::Time);
	if(!ext || !time) return false;

	const auto cfg = ext->get();
	if(!cfg.quietHours || cfg.quietStart == cfg.quietEnd) return false;

	const int hour = time->getTime().tm_hour;
	if(cfg.quietStart < cfg.quietEnd){
		return hour >= cfg.quietStart && hour < cfg.quietEnd;
	}
	return hour >= cfg.quietStart || hour < cfg.quietEnd; // wraps past midnight
}

void StatusCenter::onPhoneLost(){
	auto ext = (ExtSettings*) Services.get(Service::ExtSettings);
	auto phone = (Phone*) Services.get(Service::Phone);
	if(!ext || !phone || !ext->get().disconnectAlert || phone->isConnected() || inQuietHours()) return;

	if(settings.get().notificationSounds){
		chirp.play({
				Chirp{ .startFreq = 900, .endFreq = 500, .duration = 250 },
				Chirp{ .startFreq = 0, .endFreq = 0, .duration = 120 },
				Chirp{ .startFreq = 900, .endFreq = 500, .duration = 250 }
		});
	}
	if(settings.get().ledEnable){
		led->blinkTwice({ 255, 0, 0 });
	}
}

void StatusCenter::startAlarm(){
	if(alarmActive.exchange(true)) return;
	alarmTick();
	esp_timer_start_periodic(alarmTimer, 1500 * 1000);
}

void StatusCenter::stopAlarm(){
	if(!alarmActive.exchange(false)) return;
	esp_timer_stop(alarmTimer);
	updateLED();
}

void StatusCenter::alarmTick(){
	if(!alarmActive) return;

	// Sound comes from the MelodyPlayer; this is the visual part
	if(settings.get().ledEnable){
		led->blinkTwice({ 255, 255, 255 });
		for(const auto& singleLed : singleLeds){
			singleLed->blinkTwice(0xFF);
		}
	}
}

void StatusCenter::processBatt(const Battery::Event& evt){
	if(evt.action == Battery::Event::Charging){
		if(evt.chargeStatus == Battery::ChargingState::Full){
			battState = Full;
		}else if(evt.chargeStatus == Battery::ChargingState::Charging){
			battState = Charging;
		}
	}else{
		auto level = evt.level;

		auto battery = (Battery*) Services.get(Service::Battery);
		if(level == Battery::Critical && battery && battery->isShutdown()){
			shutdown();
			stop(0);
			return;
		}else if(level == Battery::VeryLow || level == Battery::Critical){
			battState = Empty;
		}else{
			battState = Ok;
		}
	}

	updateLED();
}

void StatusCenter::updateLED(){
	if(!settings.get().ledEnable){
		led->clear();
		for(const std::shared_ptr<DigitalLEDController>& singleLed : singleLeds){
			singleLed->clear();
		}
		return;
	}

	if(battState == Empty){
		led->blinkContinuous({ 255, 0, 0 }, -1, 100, 3000);
	}else if(battState == Charging){
		led->breathe({ 100, 10, 0 }, { 150, 50, 0 }, 3000);
	}else if(battState == Full){
		led->breathe({ 0, 50, 0 }, { 0, 150, 0 }, 3000);
	}else if(battState == Ok){
		led->clear();
	}
}

void StatusCenter::blockAudio(bool block){
	this->audioBlocked = block;
}

void StatusCenter::blink(){
	if(settings.get().ledEnable == false) return;

	led->blinkTwice({ 0, 0, 255 });

	for(const std::shared_ptr<DigitalLEDController>& singleLed : singleLeds){
		singleLed->blinkTwice(0xFF);
	}
}

void StatusCenter::circularBlink(){
	if(settings.get().ledEnable == false) return;

	for(const std::shared_ptr<DigitalLEDController>& singleLed : singleLeds){
		singleLed->blinkTwice(0xFF, 100, 300);
		vTaskDelay(50);
	}
}

void StatusCenter::beep(){
	chirp.play({
		Chirp{ .startFreq = 600, .endFreq = 600, .duration = 50 },
		Chirp{ .startFreq = 0, .endFreq = 0, .duration = 100 },
		Chirp{ .startFreq = 400, .endFreq = 400, .duration = 50 }
	});
}

void StatusCenter::shutdown(){
	led->setSolid({ 150, 0, 0 });
	vTaskDelay(SleepMan::ShutdownTime-100);

	led->breathe({ 150, 0, 0 }, { 0, 0, 0 }, 1000);
	vTaskDelay(500);

	for(const auto& singleLed : singleLeds){
		singleLed->clear();
	}
	led->clear();
}

void StatusCenter::blinkUp(){
	if(settings.get().ledEnable == false) return;

	auto* settings = (Settings*) Services.get(Service::Settings);
	if(settings->get().screenRotate){
		singleLeds[4]->blink(0xff);
		singleLeds[5]->blink(0xff);
	}else{
		singleLeds[0]->blink(0xff);
		singleLeds[1]->blink(0xff);
	}
}

void StatusCenter::blinkDown(){
	if(settings.get().ledEnable == false) return;

	auto* settings = (Settings*) Services.get(Service::Settings);
	if(settings->get().screenRotate){
		singleLeds[0]->blink(0xff);
		singleLeds[1]->blink(0xff);
	}else{
		singleLeds[4]->blink(0xff);
		singleLeds[5]->blink(0xff);
	}
}

void StatusCenter::blinkRand(){
	if(settings.get().ledEnable == false) return;

	std::unordered_set<int> leds;
	while(leds.size() < 2){
		leds.insert(rand()%6);
	}

	for(int led : leds){
		singleLeds[led]->blink(0xff);
	}
}

void StatusCenter::blinkAll(){
	if(settings.get().ledEnable == false) return;

	for(const auto& singleLed : singleLeds){
		singleLed->blink(0xff);
	}
}

void StatusCenter::blinkAllTwice(){
	if(settings.get().ledEnable == false) return;

	for(const auto& singleLed : singleLeds){
		singleLed->blinkTwice(0xff);
	}
}
