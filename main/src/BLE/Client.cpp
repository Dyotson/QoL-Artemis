#include "Client.h"
#include "GAP.h"
#include "ConMan.h"
#include "Services/Diagnostics.h"
#include "Util/Services.h"
#include <cstring>
#include <algorithm>
#include <esp_log.h>
#include <esp_gap_ble_api.h>

static const char* TAG = "BLE::Client";

BLE::Client* BLE::Client::self = nullptr;

BLE::Client::Client(GAP* gap) : gap(gap){
	if(self != nullptr){
		ESP_LOGE(TAG, "Client already exists");
		return;
	}
	self = this;

	esp_ble_gattc_register_callback([](esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t* param){
		if(self == nullptr) return;
		self->ble_GATTC_cb(event, gattc_if, param);
	});

	if(iface.appID == 0xff){
		iface.appID = AppID;
		esp_ble_gattc_app_register(AppID);
	}

	// TODO: This is only needed so GAP can notify the GATT Client when pairing is done
	gap->setClient(this);

	const esp_timer_create_args_t args = {
			.callback = [](void* arg){ static_cast<Client*>(arg)->onDiscoveryTimeout(); },
			.arg = this,
			.dispatch_method = ESP_TIMER_TASK,
			.name = "GATTCDisc",
			.skip_unhandled_events = true
	};
	esp_timer_create(&args, &discoveryTimer);

	esp_timer_create_args_t encArgs = args;
	encArgs.callback = [](void* arg){ static_cast<Client*>(arg)->onEncryptTimeout(); };
	encArgs.name = "GATTCEnc";
	esp_timer_create(&encArgs, &encryptTimer);
}

BLE::Client::~Client(){
	self = nullptr;
	gap->setClient(nullptr);
}

void BLE::Client::setRecoveryAllowed(std::function<bool()> fn){
	recoveryAllowed = std::move(fn);
}

std::shared_ptr<BLE::Client::Service> BLE::Client::addService(esp_bt_uuid_t uuid){
	std::shared_ptr<Service> srv(new Service(uuid));
	services.insert(srv);
	return srv;
}

void BLE::Client::ble_GATTC_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t* param){
	/* 0xff, not specify a certain gatt_if, need to call every profile cb function */
	if(gattc_if == 0xff){
		printf("GATTC CB interface 0xff - event %d\n", event);
	}

	if(event == ESP_GATTC_REG_EVT){
		ESP_LOGI(TAG, "ESP_GATTC_REG_EVT");

		if(param->reg.app_id != iface.appID){
			ESP_LOGE(TAG, "CB: App ID missmatch. Received %d, have %d. Status is %d", param->reg.app_id, iface.appID, param->reg.status);
			return;
		}

		if(param->reg.status == ESP_GATT_OK){
			ESP_LOGI(TAG, "App registered");
			iface.hndl = gattc_if;
		}else{
			ESP_LOGW(TAG, "Reg app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
			iface = {};
			return;
		}
	}else if(event == ESP_GATTC_CONNECT_EVT){
		ESP_LOGI(TAG, "ESP_GATTC_CONNECT_EVT");
		onConnect(&param->connect);
	}else if(event == ESP_GATTC_OPEN_EVT){
		ESP_LOGI(TAG, "ESP_GATTC_OPEN_EVT");
		onOpen(&param->open);
	}else if(event == ESP_GATTC_CFG_MTU_EVT){
		ESP_LOGI(TAG, "ESP_GATTC_CFG_MTU_EVT");
		onMtuResp(&param->cfg_mtu);
	}else if(event == ESP_GATTC_SEARCH_RES_EVT){
		ESP_LOGI(TAG, "ESP_GATTC_SEARCH_RES_EVT");
		onSearchResult(&param->search_res);
	}else if(event == ESP_GATTC_SEARCH_CMPL_EVT){
		ESP_LOGI(TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
		onSearchComplete(&param->search_cmpl);
	}else if(event == ESP_GATTC_CLOSE_EVT){
		ESP_LOGI(TAG, "ESP_GATTC_CLOSE_EVT");
		onClose(&param->close);
	}else if(event == ESP_GATTC_DISCONNECT_EVT){
		ESP_LOGI(TAG, "ESP_GATTC_DISCONNECT_EVT");
		onDisconnect(&param->disconnect);
	}else{
		switch(event){
			case ESP_GATTC_REG_FOR_NOTIFY_EVT:
			case ESP_GATTC_NOTIFY_EVT:
			case ESP_GATTC_WRITE_CHAR_EVT:
			case ESP_GATTC_READ_CHAR_EVT:
				passToChar(event, param);
				break;
			default:
				ESP_LOGV(TAG, "Unhandled event: %d", event);
				break;
		}
	}
}

void BLE::Client::onConnect(const esp_ble_gattc_cb_param_t::gattc_connect_evt_param* param){
	memcpy(con.addr, param->remote_bda, 6);

	// Capture the conn_id now so onPairDone() has a valid handle even if the
	// AUTH_CMPL event arrives before ESP_GATTC_OPEN_EVT (see maybeStartDiscovery).
	con.hndl = param->conn_id;
	con.opened = false;
	con.paired = authOnLink;
	con.discovering = false;

	// Run the setup (encryption, discovery, subscriptions) on a fast link interval, even
	// when the screen is off: at the low-power interval it takes many seconds.
	ConMan.setBoost(true);

	// Server notifies ConMan, which in turn sets connection parameters

	// Open the GATT client connection immediately so a control block (clcb) exists
	// from link-up. A bonded iPhone re-uses its persisted ANCS CCCD subscriptions
	// and starts pushing notifications as soon as the link encrypts; if the open
	// is deferred until after pairing, those notifications arrive before the clcb
	// exists and bluedroid drops them ("indication/notif for unknown device,
	// ignore"). The ACL already exists here, so this only creates the virtual GATT
	// connection. Encryption is started from onOpen, once the clcb is in place.
	esp_ble_gattc_open(iface.hndl, con.addr, BLE_ADDR_TYPE_PUBLIC, true);
}

void BLE::Client::onPairDone(){
	// Link is now encrypted. Discovery proceeds once the connection is also open.
	authOnLink = true;
	con.paired = true;
	if(auto diag = (Diagnostics*) Services.get(::Service::Diag)){
		diag->log(Diagnostics::Ev::Encrypted);
	}
	if(encryptTimer) esp_timer_stop(encryptTimer);
	maybeStartDiscovery();
}

void BLE::Client::onOpen(const esp_ble_gattc_cb_param_t::gattc_open_evt_param* param){
	if(param->status != ESP_GATT_OK){
		ESP_LOGE(TAG, "open failed, error status = 0x%x", param->status);
		return;
	}

	con.hndl = param->conn_id;
	con.opened = true;

	// Now that the GATT client connection (clcb) exists, start pairing. When it
	// completes, ESP_GAP_BLE_AUTH_CMPL_EVT fires (handled by BLE) and BLE calls
	// onPairDone(). Discovery proceeds once the link is also encrypted.
	if(!con.paired){
		esp_ble_set_encryption(con.addr, ESP_BLE_SEC_ENCRYPT_MITM);
		encryptRetries = 0;
		if(encryptTimer){
			esp_timer_stop(encryptTimer);
			esp_timer_start_once(encryptTimer, EncryptTimeoutUs);
		}
	}

	maybeStartDiscovery();
}

void BLE::Client::onEncryptTimeout(){
	if(!con || !con.opened || con.paired) return;
	if(encryptRetries >= MaxRetries) return;

	encryptRetries++;
	ESP_LOGW(TAG, "Link not encrypted yet, requesting again (%d)", encryptRetries);
	if(auto diag = (Diagnostics*) Services.get(::Service::Diag)){
		diag->log(Diagnostics::Ev::EncRetry, encryptRetries);
	}
	esp_ble_set_encryption(con.addr, ESP_BLE_SEC_ENCRYPT_MITM);
	esp_timer_start_once(encryptTimer, EncryptTimeoutUs);
}

void BLE::Client::onDiscoveryTimeout(){
	if(!con || !con.discovering) return;

	if(searchPending){
		searchPending = false;
		searchServices();
		esp_timer_start_once(discoveryTimer, DiscoveryTimeoutUs);
		return;
	}

	// Done only when every registered service is set up: with just the clock service up
	// and not the notification service, the watch would sit there without notifications.
	bool complete = true;
	for(const auto& svc : services){
		if(!svc->populated()) complete = false;
	}
	if(complete){
		// Setup traffic is over, back to normal link parameters
		ConMan.setBoost(false);
		return;
	}

	if(recoveryAllowed && !recoveryAllowed()){
		// e.g. an Android phone: it talks to our GATT server and has none of these services
		ConMan.setBoost(false);
		return;
	}

	// Keep trying for as long as the link is up, backing off (5 s, 10 s, 20 s, ... 60 s).
	// Not dropping the link on purpose: iOS backs off from reconnecting to an accessory that
	// keeps closing the connection, which is worse than retrying.
	if(discoveryRetries < 0xff) discoveryRetries++;
	ESP_LOGW(TAG, "Services not all set up, searching again (%d)", discoveryRetries);
	if(auto diag = (Diagnostics*) Services.get(::Service::Diag)){
		diag->log(Diagnostics::Ev::DiscRetry, discoveryRetries);
	}
	searchServices();
	const uint64_t backoff = std::min<uint64_t>(DiscoveryTimeoutUs << std::min<int>(discoveryRetries - 1, 4), 60ULL * 1000 * 1000);
	esp_timer_start_once(discoveryTimer, backoff);
}

void BLE::Client::maybeStartDiscovery(){
	// Begin MTU negotiation (then, via onMtuResp, service discovery) only once the
	// GATT connection is open AND the link is encrypted. ESP_GATTC_OPEN_EVT and
	// ESP_GAP_BLE_AUTH_CMPL_EVT can arrive in either order — for a bonded iPhone
	// the controller may auto-encrypt before the open completes — so whichever
	// finishes last triggers discovery. `discovering` ensures it starts once.
	if(!con.opened || !con.paired || con.discovering) return;
	con.discovering = true;

	discoveryRetries = 0;

	// No MTU request of our own: the ATT MTU is shared by both directions of the link and
	// iOS always negotiates it as soon as it connects. When the phone got there first, our
	// request was never answered, and every discovery request queued up behind it - the
	// link stayed up while ANCS was never found.
	//
	// The search itself starts a moment later: right after encryption the iPhone is still
	// busy with its own setup traffic (MTU, discovering our server) and an immediate search
	// often failed, costing a full retry cycle.
	searchPending = true;
	if(discoveryTimer){
		esp_timer_stop(discoveryTimer);
		esp_timer_start_once(discoveryTimer, FirstSearchDelayUs);
	}else{
		searchPending = false;
		searchServices();
	}
}

void BLE::Client::onMtuResp(const esp_ble_gattc_cb_param_t::gattc_cfg_mtu_evt_param* param){
	if(param->status != ESP_GATT_OK){
		// Expected when the iPhone already ran its own MTU exchange on this link (it races
		// ours right after connecting). The stock code stopped here, leaving the link up
		// but ANCS never discovered: iOS showed "Connected" while the watch didn't.
		ESP_LOGW(TAG, "config mtu failed (status 0x%x), continuing with discovery", param->status);
	}else{
		con.MTU_size = param->mtu;
	}

	searchServices();
}

void BLE::Client::searchServices(){
	// One "discover all primary services" request; onSearchResult matches the results to
	// our registered services. Issuing one filtered search per service back to back got
	// all but the first rejected, since the stack runs a single discovery at a time.
	esp_ble_gattc_search_service(iface.hndl, con.hndl, nullptr);
}

void BLE::Client::onSearchResult(const esp_ble_gattc_cb_param_t::gattc_search_res_evt_param* param){
	for(const auto& service : services){
		if(service->populated()) continue; // already set up by an earlier search (retries search everything)
		if(param->srvc_id.uuid.len == service->uuid.len && memcmp(param->srvc_id.uuid.uuid.uuid128, service->uuid.uuid.uuid128, service->uuid.len) == 0){
			service->establish(std::make_unique<ServiceInfo>(this, param->start_handle, param->end_handle));
		}
	}
}

void BLE::Client::onSearchComplete(const esp_ble_gattc_cb_param_t::gattc_search_cmpl_evt_param* param){
	if(param->status != ESP_GATT_OK){
		ESP_LOGE(TAG, "search service failed, error status = %x", param->status);
		if(auto diag = (Diagnostics*) Services.get(::Service::Diag)){
			diag->log(Diagnostics::Ev::SearchFail, param->status);
		}
		return;
	}

	// TODO: invoke pull on the service which search results belong to
	// current implementation only works with one service registered, I think
	for(auto& svc : services){
		if(!svc->established()) continue;
		if(svc->populated()) continue;
		svc->pull();
	}

}

void BLE::Client::onClose(const esp_ble_gattc_cb_param_t::gattc_close_evt_param* param){
	if(param->status != ESP_GATT_OK){
		ESP_LOGE(TAG, "close failed, error status = %x", param->status);
		return;
	}

	close();
}

void BLE::Client::onDisconnect(const esp_ble_gattc_cb_param_t::gattc_disconnect_evt_param* param){
	ESP_LOGI(TAG, "Disconnected. Reason: 0x%x", param->reason);
	close();
}

void BLE::Client::passToChar(esp_gattc_cb_event_t event, esp_ble_gattc_cb_param_t* param){
#define check(x) do { if(x == chars.end()){ ESP_LOGW(TAG, "Received event %d directed to non-registered characteristic", event); return; } } while(0)

	if(event == ESP_GATTC_REG_FOR_NOTIFY_EVT){
		ESP_LOGI(TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");

		auto chr = chars.find(param->reg_for_notify.handle);
		check(chr);

		chr->second->onRegNotify(&param->reg_for_notify);
	}else if(event == ESP_GATTC_NOTIFY_EVT){
		ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT");

		auto chr = chars.find(param->notify.handle);
		check(chr);

		chr->second->onNotify(&param->notify);
	}else if(event == ESP_GATTC_READ_CHAR_EVT){
		ESP_LOGI(TAG, "ESP_GATTC_READ_CHAR_EVT");

		auto chr = chars.find(param->read.handle);
		check(chr);

		// TODO: lol what is this, turn this into actual read evt handling
		esp_ble_gattc_cb_param_t::gattc_notify_evt_param notifParam = {
				.value_len = param->read.value_len,
				.value = param->read.value,
				.is_notify = true
		};
		chr->second->onNotify(&notifParam);
	}else if(event == ESP_GATTC_WRITE_CHAR_EVT){
		ESP_LOGI(TAG, "ESP_GATTC_WRITE_CHAR_EVT");

		auto chr = chars.find(param->write.handle);
		check(chr);

		chr->second->onWriteResp(event, &param->write);

	}else{
		ESP_LOGW(TAG, "Unhandled characteristic event: 0x%x", event);
	}
}

void BLE::Client::close(){
	for(auto& svc : services){
		if(svc == nullptr) {
			continue;
		}

		svc->close();
	}
	chars.clear();

	// Reset to the "no connection" sentinel that ConnectionInfo::operator bool
	// checks (0xffff), not 0 — otherwise a closed connection still reads as valid.
	if(discoveryTimer) esp_timer_stop(discoveryTimer);
	if(encryptTimer) esp_timer_stop(encryptTimer);
	authOnLink = false;

	con.hndl = 0xffff;
	con.opened = false;
	con.paired = false;
	con.discovering = false;
	searchPending = false;
	memset(con.addr, 0, 6);

	// Server notifies ConMan, which in turn starts advertising
}

BLE::Client::Status BLE::Client::getStatus(){
	Status st = {
			.linked = (bool) con,
			.opened = con.opened,
			.paired = con.paired,
			.discovering = con.discovering,
			.established = 0,
			.populated = 0
	};
	for(const auto& svc : services){
		if(svc->established()) st.established++;
		if(svc->populated()) st.populated++;
	}
	return st;
}
