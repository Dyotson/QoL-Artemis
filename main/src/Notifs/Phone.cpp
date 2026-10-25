#include "Phone.h"
#include "Util/Events.h"
#include "Util/stdafx.h"
#include <functional>
#include <esp_log.h>

static const char* TAG = "Phone";

Phone::Phone(BLE::Server* server, BLE::Client* client) : ancs(client), ams(client), cTime(client), android(server){
	auto reg = [this](NotifSource* src){
		src->setOnConnect([this, src](){ onConnect(src); });
		src->setOnDisconnect([this, src](){ onDisconnect(src); });
		src->setOnNotifAdd([this](Notif notif){ onAdd(std::move(notif)); });
		src->setOnNotifModify([this](Notif notif){ onModify(std::move(notif)); });
		src->setOnNotifRemove([this](uint32_t id){ onRemove(id); });
	};

	reg(&ancs);
	reg(&android);

	auto mreg = [this](MediaSource* src){
		src->setOnConnect([this, src](){ onMediaConnect(src); });
		src->setOnDisconnect([this, src](){ onMediaDisconnect(src); });
		src->setOnMediaInfo([this](const MediaInfo& media){ onMediaInfo(media); });
		src->setOnMediaState([this](MediaState state){ onMediaState(state); });
	};

	mreg(&android);
	mreg(&ams);

	const esp_timer_create_args_t args = {
			.callback = [](void* arg){ static_cast<Phone*>(arg)->sweepStale(); },
			.arg = this,
			.dispatch_method = ESP_TIMER_TASK,
			.name = "NotifSweep",
			.skip_unhandled_events = true
	};
	esp_timer_create(&args, &sweepTimer);
}

bool Phone::isConnected(){
	return current != nullptr;
}

Phone::PhoneType Phone::getPhoneType(){
	if(current == &ancs) return PhoneType::IPhone;
	else if(current == &android) return PhoneType::Android;
	else return PhoneType::None;
}

auto Phone::findNotif(uint32_t id){
	return std::find_if(notifs.begin(), notifs.end(), [id](const auto& notif){ return notif.uid == id; });
}

Notif Phone::getNotif(uint32_t uid){
	std::lock_guard lock(mut);
	auto notif = findNotif(uid);
	if(notif == notifs.end()) return {};
	return *notif;
}

std::vector<Notif> Phone::getNotifs(){
	std::lock_guard lock(mut);
	return std::vector<Notif>(notifs.cbegin(), notifs.cend());
}

uint32_t Phone::getNotifsCount() const{
	std::lock_guard lock(mut);
	return notifs.size();
}

Notif Phone::getCall(){
	std::lock_guard lock(mut);
	for(const auto& notif : notifs){
		if(notif.category == Notif::Category::IncomingCall){
			return notif;
		}
	}

	return {};
}

void Phone::callIgnore(uint32_t uid){
	std::lock_guard lock(mut);
	auto notif = findNotif(uid);
	if(notif == notifs.end()) return;
	notifs.erase(notif);
}

void Phone::callReject(uint32_t uid){
	{
		std::lock_guard lock(mut);
		auto notif = findNotif(uid);
		if(notif == notifs.end()) return;
		notifs.erase(notif);
	}

	if(current == &android){
		current->actionNeg(uid);
	}
}

const MediaInfo& Phone::getMedia() const{
	return currentMedia;
}

MediaState Phone::getMediaState(){
	return currentMediaState;
}

void Phone::doPos(uint32_t id){
	{
		std::lock_guard lock(mut);
		if(current == nullptr || findNotif(id) == notifs.end() || stale.count(id)) return;
	}
	current->actionPos(id);
}

void Phone::doNeg(uint32_t id){
	{
		std::lock_guard lock(mut);
		if(current == nullptr || findNotif(id) == notifs.end() || stale.count(id)) return;
	}
	current->actionNeg(id);
}

void Phone::doMediaPlay(){
	if(mediaCurrent == nullptr) return;
	mediaCurrent->mediaPlay();
}

void Phone::doMediaPause(){
	if(mediaCurrent == nullptr) return;
	mediaCurrent->mediaPause();
}

void Phone::doMediaNext(){
	if(mediaCurrent == nullptr) return;
	mediaCurrent->mediaNext();
}

void Phone::doMediaPrev(){
	if(mediaCurrent == nullptr) return;
	mediaCurrent->mediaPrev();
}

bool Phone::takeAlert(){
	const auto now = millis();
	if(lastAlertMs != 0 && now - lastAlertMs < AlertCooldownMs) return false;
	lastAlertMs = now;
	return true;
}

void Phone::removeLocked(std::deque<Notif>::iterator it){
	const uint32_t uid = it->uid;
	notifs.erase(it);
	stale.erase(uid);
	Events::post(Facility::Phone, Event { .action = Event::Removed, .data = { .addChgRem = { .id = uid } } });
}

bool Phone::matchesStale(const Notif& notif){
	for(auto it = notifs.begin(); it != notifs.end(); ++it){
		if(!stale.count(it->uid)) continue;
		if(it->appID == notif.appID && it->title == notif.title && it->message == notif.message){
			removeLocked(it);
			return true;
		}
	}
	return false;
}

void Phone::onConnect(NotifSource* src){
	{
		std::lock_guard lock(mut);
		current = src;
		connectMs = millis();

		// Everything we kept from the previous session is now waiting to be confirmed by
		// the phone re-sending it. Unconfirmed ones are swept once the grace period ends.
		for(const auto& notif : notifs){
			stale.insert(notif.uid);
		}
	}

	Events::post(Facility::Phone, Event { .action = Event::Connected, .data = { .phoneType = getPhoneType() } });

	if(sweepTimer){
		esp_timer_stop(sweepTimer);
		esp_timer_start_once(sweepTimer, StaleGraceMs * 1000);
	}

	currentMediaState = MediaState::Stopped;
	currentMedia = {};
}

void Phone::onDisconnect(NotifSource* src){
	if(current != src) return;
	Events::post(Facility::Phone, Event { .action = Event::Disconnected, .data = { .phoneType = getPhoneType() } });

	std::lock_guard lock(mut);
	current = nullptr;
	hadSession = true;

	if(sweepTimer){
		esp_timer_stop(sweepTimer);
	}

	// Keep notifications readable while disconnected, except calls which can't be
	// answered any more (and would otherwise reopen the call screen).
	for(auto it = notifs.begin(); it != notifs.end();){
		if(it->category == Notif::Category::IncomingCall){
			const uint32_t uid = it->uid;
			it = notifs.erase(it);
			stale.erase(uid);
			Events::post(Facility::Phone, Event { .action = Event::Removed, .data = { .addChgRem = { .id = uid } } });
		}else{
			stale.insert(it->uid);
			++it;
		}
	}
}

void Phone::sweepStale(){
	std::lock_guard lock(mut);
	if(current == nullptr) return; // still disconnected: keep showing them

	for(auto it = notifs.begin(); it != notifs.end();){
		if(stale.count(it->uid)){
			const uint32_t uid = it->uid;
			it = notifs.erase(it);
			stale.erase(uid);
			Events::post(Facility::Phone, Event { .action = Event::Removed, .data = { .addChgRem = { .id = uid } } });
		}else{
			++it;
		}
	}
}

void Phone::onMediaConnect(MediaSource* src){
	mediaCurrent = src;
	Events::post(Facility::Phone, Event { .action = Event::MediaConnected });
}

void Phone::onMediaDisconnect(MediaSource* src){
	if(mediaCurrent != src) return;
	mediaCurrent = nullptr;
	Events::post(Facility::Phone, Event { .action = Event::MediaDisconnected });

	currentMediaState = MediaState::Stopped;
	currentMedia = {};
}

void Phone::onMediaInfo(const MediaInfo& media){
	currentMedia = media;
	Events::post(Facility::Phone, Event { .action = Event::MediaInfo });
}

void Phone::onMediaState(MediaState state){
	currentMediaState = state;
	Events::post(Facility::Phone, Event { .action = Event::MediaState, .data = { .mediaState = getMediaState() } });
}

void Phone::onAdd(Notif notif){
	if(notif.title.empty() && notif.message.empty()) return;

	std::lock_guard lock(mut);

	auto existing = findNotif(notif.uid);
	if(existing != notifs.end()){
		if(stale.count(notif.uid)){
			// UID reused by the phone's new session for a different notification
			removeLocked(existing);
		}else{
			onModify(std::move(notif));
			return;
		}
	}

	// A re-sent copy of something we already showed: replace it without alerting
	const bool replacedStale = !stale.empty() && matchesStale(notif);

	bool alert;
	if(notif.category == Notif::Category::IncomingCall){
		alert = true;
	}else if(notif.silent || replacedStale){
		alert = false;
	}else if(notif.preExisting){
		// Arrived on the phone while we were disconnected. Worth one alert, unless this is
		// the first connection after boot (then everything on the phone is "pre-existing").
		alert = hadSession && takeAlert();
	}else{
		alert = takeAlert();
	}

	while(notifs.size() >= MaxNotifs){
		removeLocked(notifs.begin());
	}

	const uint32_t uid = notif.uid;
	notifs.push_back(std::move(notif));
	Events::post(Facility::Phone, Event { .action = Event::Added, .data = { .addChgRem = { .id = uid } }, .alert = alert });
}

void Phone::onModify(Notif notif){
	std::lock_guard lock(mut);

	auto saved = findNotif(notif.uid);
	if(saved == notifs.end() || stale.count(notif.uid)){
		onAdd(std::move(notif));
		return;
	}

	// iOS sends lots of modifications that don't change anything visible (timers,
	// navigation, now-playing...). Only alert when the text actually changed.
	const bool changed = saved->title != notif.title || saved->message != notif.message;
	const bool alert = changed && !notif.silent && (notif.category == Notif::Category::IncomingCall || takeAlert());

	*saved = notif;
	Events::post(Facility::Phone, Event { .action = Event::Changed, .data = { .addChgRem = { .id = notif.uid } }, .alert = alert });
}

void Phone::onRemove(uint32_t id){
	std::lock_guard lock(mut);
	auto notif = findNotif(id);
	if(notif == notifs.end()) return;

	removeLocked(notif);
}

void Phone::findPhoneStart(){
	if(current != &android) return;
	android.findPhoneStart();
}

void Phone::findPhoneStop(){
	if(current != &android) return;
	android.findPhoneStop();
}

bool Phone::findPhoneActive(){
	if(current != &android) return false;
	return android.findPhoneActive();
}

void Phone::debugInject(const std::string& title, const std::string& message){
	Notif notif = {
			.uid = debugUid++,
			.title = title,
			.message = message,
			.appID = "Messages",
			.category = Notif::Category::Social
	};
	ESP_LOGI(TAG, "Injecting test notification 0x%lx", notif.uid);
	onAdd(std::move(notif));
}
