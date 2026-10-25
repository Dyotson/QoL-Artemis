#ifndef CLOCKSTAR_FIRMWARE_CONMAN_H
#define CLOCKSTAR_FIRMWARE_CONMAN_H

#include "GAP.h"
#include "ConConf.h"
#include <esp_gap_ble_api.h>
#include <esp_timer.h>
#include <mutex>

class ConManager {
public:

	// Public methods are invoked from the BLE event task (GAP/GATTS callbacks) and from
	// the sleep code; the advertising schedule additionally runs from an esp_timer. All
	// internal state is guarded by `mut`.
	void connect(const esp_bd_addr_t addr);
	void disconnect();

	// Called once the GAP config (adv/scan-response data) is ready, to kick off advertising.
	void start();

	void goLowPow();
	void goHiPow();

private:
	friend BLE::GAP;
	void confDone(bool success);
	ConConf conConf;

	std::recursive_mutex mut;

	bool connected = false;
	esp_bd_addr_t current;

	// `advertising` is the host-side hint for whether the radio is currently advertising;
	// it is *not* a faithful mirror of bluedroid's internal state. It is set on
	// ADV_START/ADV_STOP complete events, and additionally cleared in connect() to cover
	// bluedroid's implicit advertising stop on a connection event (which does not always
	// surface an ADV_STOP_COMPLETE_EVT). `wantAdv` is the requested intent set by
	// start/stopAdv. startAdv() / stopAdv() short-circuit when `advertising` already
	// matches intent, so this hint must stay synchronized at every implicit-state-change
	// site.
	bool advertising = false;
	bool wantAdv = false;

	bool lowPow = false;

	void startAdv();
	void stopAdv();
	void setCon();

	// Fired by GAP from ADV_START/ADV_STOP complete events on the BLE/BTC task.
	void onAdvStartComplete(bool success);
	void onAdvStopComplete(bool success);

	/**
	 * Advertising schedule after a disconnect (and at boot), following Apple's accessory
	 * guidelines: advertise fast for 30 s so a nearby iPhone reconnects within a second or
	 * two, then step down to Apple's recommended longer intervals. iOS scans for known
	 * accessories with a low duty cycle in the background, so advertising at the slowest
	 * interval right away (as the stock firmware did while the screen was off) made
	 * reconnects take minutes.
	 */
	enum class AdvStage : uint8_t { Fast, Medium, Slow };
	AdvStage advStage = AdvStage::Fast;
	esp_timer_handle_t advTimer = nullptr;
	void setAdvStage(AdvStage stage);
	void onAdvTimer();
	const esp_ble_adv_params_t* advParams() const;

	static constexpr uint64_t FastAdvDuration = 30ULL * 1000 * 1000;	// [us]
	static constexpr uint64_t MediumAdvDuration = 150ULL * 1000 * 1000;	// [us]

	static constexpr esp_ble_adv_params_t AdvFast = {
			.adv_int_min        = 32, // 20ms = 32 * 0.625ms
			.adv_int_max        = 32,
			.adv_type           = ADV_TYPE_IND,
			.own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
			.channel_map        = ADV_CHNL_ALL,
			.adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
	};

	static constexpr esp_ble_adv_params_t AdvMedium = {
			.adv_int_min        = 244, // 152.5ms = 244 * 0.625ms (Apple recommended)
			.adv_int_max        = 244,
			.adv_type           = ADV_TYPE_IND,
			.own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
			.channel_map        = ADV_CHNL_ALL,
			.adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
	};

	static constexpr esp_ble_adv_params_t AdvSlow = {
			.adv_int_min        = 668, // 417.5ms = 668 * 0.625ms (Apple recommended)
			.adv_int_max        = 668,
			.adv_type           = ADV_TYPE_IND,
			.own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
			.channel_map        = ADV_CHNL_ALL,
			.adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
	};

	/*
	 * Connection parameters, within Apple's accessory rules:
	 *   interval min >= 15ms and a multiple of 15ms, interval max >= min + 15ms,
	 *   peripheral latency <= 30, 2s <= supervision timeout <= 6s,
	 *   interval max * (latency + 1) <= 2s, and * 3 < supervision timeout.
	 *
	 * Screen off: the stock firmware used a ~1.1s interval with no latency, which leaves
	 * only ~5 connection events before the 6s supervision timeout - a few seconds of the
	 * phone being blocked by your body (pocket, other wrist) dropped the link. With a
	 * ~0.4s interval and peripheral latency 2 the watch still only wakes every ~1.2s when
	 * idle, but can resync on the very next event after a miss, giving ~3x more chances
	 * before the link times out.
	 */
	static constexpr esp_ble_conn_update_params_t ConLowPow = {
			.min_int = 288, // 288*1.25ms = 360ms
			.max_int = 336, // 336*1.25ms = 420ms
			.latency = 2,
			.timeout = 600  // 600*10ms = 6000ms
	};

	static constexpr esp_ble_conn_update_params_t ConHiPow = {
			.min_int = 24, // 24*1.25ms = 30ms
			.max_int = 40, // 40*1.25ms = 50ms
			.latency = 0,
			.timeout = 500 // 500*10ms = 5000ms
	};

};

extern ConManager ConMan;

#endif //CLOCKSTAR_FIRMWARE_CONMAN_H
