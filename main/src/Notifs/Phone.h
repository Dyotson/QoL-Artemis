#ifndef CLOCKSTAR_FIRMWARE_PHONE_H
#define CLOCKSTAR_FIRMWARE_PHONE_H

#include "Android.h"
#include "ANCS/Client.h"
#include "AMS/Client.h"
#include "CurrentTime.h"
#include "NotifSource.h"
#include "MediaSource.h"
#include <deque>
#include <cstdint>
#include <optional>
#include <mutex>
#include <unordered_set>
#include <esp_timer.h>

class Phone {
public:

	enum class PhoneType {
		None, Android, IPhone
	};

	struct Event {
		enum { Connected, Disconnected, Added, Changed, Removed, Cleared,
		      MediaConnected, MediaDisconnected, MediaState, MediaInfo } action;
		union {
			struct { uint32_t id; } addChgRem;
			PhoneType phoneType;
			::MediaState mediaState;
		} data;

		// Added/Changed: whether the user should be alerted (beep/LEDs) for this event.
		// False for silent, pre-existing (re-sent on reconnect) and rate-limited updates.
		bool alert = false;
	};

	static constexpr size_t MaxNotifs = 20;

	Phone(BLE::Server* server, BLE::Client* client);

	bool isConnected();
	PhoneType getPhoneType();

	Notif getNotif(uint32_t uid);
	std::vector<Notif> getNotifs();
	uint32_t getNotifsCount() const;

	Notif getCall();
	void callIgnore(uint32_t uid);
	void callReject(uint32_t uid);

	void doPos(uint32_t id);
	void doNeg(uint32_t id);

	// Media controls
	void doMediaPlay();
	void doMediaPause();
	void doMediaNext();
	void doMediaPrev();

	const MediaInfo& getMedia() const;
	MediaState getMediaState();

	void findPhoneStart();
	void findPhoneStop();
	bool findPhoneActive();

	const CurrentTime& getCurrentTime() const { return cTime; }

	/** Adds a fake notification, for testing the UI from the dev console. */
	void debugInject(const std::string& title, const std::string& message);

private:
	ANCS::Client ancs;
	AMS::Client ams;
	CurrentTime cTime;
	Android android;

	NotifSource* current = nullptr;
	MediaSource* mediaCurrent = nullptr;

	MediaInfo currentMedia;
	MediaState currentMediaState = MediaState::Stopped;

	void onConnect(NotifSource* src);
	void onDisconnect(NotifSource* src);

	void onMediaConnect(MediaSource* src);
	void onMediaDisconnect(MediaSource* src);

	void onMediaInfo(const MediaInfo& media);
	void onMediaState(MediaState state);

	void onAdd(Notif notif);
	void onModify(Notif notif);
	void onRemove(uint32_t id);

	// Guards notifs and the stale-tracking state. Sources call in from their own tasks
	// (ANCS workers, Android, BTC), the UI reads from the LVGL task.
	mutable std::recursive_mutex mut;
	std::deque<Notif> notifs;

	auto findNotif(uint32_t id);

	/**
	 * Notifications survive a disconnect so the user can still read them. They are
	 * marked stale; on reconnect the phone re-sends what it still has (iOS flags those
	 * "pre-existing"), which replaces the stale copies. Whatever wasn't re-sent within
	 * StaleGraceMs is gone from the phone too, and gets removed.
	 */
	std::unordered_set<uint32_t> stale;
	static constexpr uint64_t StaleGraceMs = 15000;
	esp_timer_handle_t sweepTimer = nullptr;
	void sweepStale();
	void removeLocked(std::deque<Notif>::iterator it);
	bool matchesStale(const Notif& notif);

	bool hadSession = false;	// a phone connected at least once since boot
	uint64_t connectMs = 0;

	// Alert rate limiting, so a burst (e.g. reconnect re-sends) beeps at most once
	static constexpr uint64_t AlertCooldownMs = 4000;
	uint64_t lastAlertMs = 0;
	bool takeAlert();
	uint32_t debugUid = 0xF0000000;
};


#endif //CLOCKSTAR_FIRMWARE_PHONE_H
