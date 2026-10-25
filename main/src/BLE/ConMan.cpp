#include "ConMan.h"
#include <cstring>
#include <esp_log.h>

static const char* TAG = "BLE::ConMan";

ConManager ConMan;

void ConManager::confDone(bool success){
	conConf.confDone(success);
}

void ConManager::start(){
	std::lock_guard lock(mut);

	if(advTimer == nullptr){
		const esp_timer_create_args_t args = {
				.callback = [](void* arg){ static_cast<ConManager*>(arg)->onAdvTimer(); },
				.arg = this,
				.dispatch_method = ESP_TIMER_TASK,
				.name = "AdvStage",
				.skip_unhandled_events = true
		};
		esp_timer_create(&args, &advTimer);
	}

	setAdvStage(AdvStage::Fast);
	startAdv();
}

void ConManager::connect(const esp_bd_addr_t addr){
	std::lock_guard lock(mut);

	// Bluedroid implicitly stops advertising on a connection and does not always fire
	// ADV_STOP_COMPLETE_EVT for it. Sync the host-side hint here so the startAdv() /
	// stopAdv() short-circuits below match the real radio state.
	advertising = false;
	if(advTimer) esp_timer_stop(advTimer);

	connected = true;
	memcpy(current, addr, 6);
	setCon();

	stopAdv();
}

void ConManager::setBoost(bool en){
	std::lock_guard lock(mut);
	if(boost == en) return;
	boost = en;
	if(connected && lowPow){
		setCon();
	}
}

void ConManager::disconnect(){
	std::lock_guard lock(mut);

	connected = false;
	boost = false;
	conConf.reset();

	// Start over with fast advertising so the phone picks us up again quickly
	setAdvStage(AdvStage::Fast);
	startAdv();
}

void ConManager::goLowPow(){
	std::lock_guard lock(mut);
	lowPow = true;
	if(connected){
		setCon();
	}
}

void ConManager::goHiPow(){
	std::lock_guard lock(mut);
	lowPow = false;
	if(connected){
		setCon();
	}
}

const esp_ble_adv_params_t* ConManager::advParams() const{
	switch(advStage){
		case AdvStage::Fast: return &AdvFast;
		case AdvStage::Medium: return &AdvMedium;
		default: return &AdvSlow;
	}
}

void ConManager::setAdvStage(AdvStage stage){
	advStage = stage;

	if(advTimer == nullptr) return;
	esp_timer_stop(advTimer);

	if(stage == AdvStage::Fast){
		esp_timer_start_once(advTimer, FastAdvDuration);
	}else if(stage == AdvStage::Medium){
		esp_timer_start_once(advTimer, MediumAdvDuration);
	}
}

void ConManager::onAdvTimer(){
	std::lock_guard lock(mut);
	if(connected) return;

	setAdvStage(advStage == AdvStage::Fast ? AdvStage::Medium : AdvStage::Slow);
	ESP_LOGI(TAG, "Advertising interval step-down to stage %d", (int) advStage);

	// Interval can't be changed while advertising: stop, and onAdvStopComplete restarts
	// with the new parameters since wantAdv stays set.
	if(advertising && wantAdv){
		auto err = esp_ble_gap_stop_advertising();
		if(err != ESP_OK){
			ESP_LOGW(TAG, "esp_ble_gap_stop_advertising failed: 0x%x", err);
		}
	}
}

void ConManager::startAdv(){
	std::lock_guard lock(mut);
	wantAdv = true;
	if(advertising) return;
	auto err = esp_ble_gap_start_advertising((esp_ble_adv_params_t*) advParams());
	if(err != ESP_OK){
		ESP_LOGW(TAG, "esp_ble_gap_start_advertising failed: 0x%x", err);
	}
}

void ConManager::stopAdv(){
	std::lock_guard lock(mut);
	wantAdv = false;
	if(!advertising) return;
	auto err = esp_ble_gap_stop_advertising();
	if(err != ESP_OK){
		ESP_LOGW(TAG, "esp_ble_gap_stop_advertising failed: 0x%x", err);
	}
}

void ConManager::onAdvStartComplete(bool success){
	std::lock_guard lock(mut);

	if(!success){
		advertising = false;
		// Honest divergence: GAP rejected our start. Don't auto-retry here — the caller's next
		// state change (next connect/disconnect or schedule step) will issue another start.
		return;
	}
	advertising = true;

	// If our intent has flipped to "stop" between issuing the start and the event arriving,
	// pull the trigger now to converge.
	if(!wantAdv){
		auto err = esp_ble_gap_stop_advertising();
		if(err != ESP_OK){
			ESP_LOGW(TAG, "esp_ble_gap_stop_advertising failed: 0x%x", err);
		}
	}
}

void ConManager::onAdvStopComplete(bool success){
	std::lock_guard lock(mut);

	if(success){
		advertising = false;
	}

	// Resync if we still want to advertise (intent flipped, or an interval step-down)
	if(wantAdv && !advertising && !connected){
		startAdv();
	}
}

void ConManager::setCon(){
	/* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
	esp_ble_conn_update_params_t params = {};
	memcpy(&params, (lowPow && !boost) ? &ConLowPow : &ConHiPow, sizeof(esp_ble_conn_update_params_t));
	memcpy(params.bda, current, 6);
	conConf.conf(params);
}

bool ConManager::isConnected(){
	std::lock_guard lock(mut);
	return connected;
}
