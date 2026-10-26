#ifndef ARTEMIS_FIRMWARE_DIAGNOSTICS_H
#define ARTEMIS_FIRMWARE_DIAGNOSTICS_H

#include <nvs.h>
#include <esp_system.h>
#include <atomic>
#include <cstdint>
#include <string>
#include <functional>
#include <mutex>

/**
 * Keeps track of why the watch restarted and how the phone link behaves, so that
 * random resets and disconnects can be diagnosed after the fact (System info screen
 * and the USB dev console both read from here).
 */
class Diagnostics {
public:
	Diagnostics();

	static constexpr size_t BootHistory = 8;

	struct Boot {
		uint8_t reason;			// esp_reset_reason_t
		uint32_t prevUptime;	// [s] uptime reached by the previous boot (0 = unknown, e.g. power loss)
	};

	struct BootLog {
		uint32_t bootCount = 0;
		uint8_t count = 0;
		Boot boots[BootHistory] = {}; // newest first
	};

	const BootLog& getBootLog() const;
	esp_reset_reason_t getResetReason() const;

	static const char* reasonText(uint8_t reason);
	/** Whether the reason means the firmware crashed (panic/watchdog/brownout) instead of a clean restart */
	static bool reasonIsCrash(uint8_t reason);

	bool hasCoredump() const;
	/** Short "task @ PC" description of the stored crash, empty if none */
	std::string coredumpSummary() const;
	void clearCoredump();

	// Phone link statistics, updated from the BLE stack
	void onBleConnect();
	void onBleDisconnect(uint8_t reason);
	void onBleConnParams(uint16_t interval, uint16_t latency, uint16_t timeout);

	struct BleStats {
		uint32_t connects;
		uint32_t disconnects;
		uint8_t lastReason;
		uint64_t lastDisconnectMs; // millis() timestamp, 0 = never
		uint64_t lastConnectMs;
		bool connected;
		uint16_t interval;	// [1.25ms]
		uint16_t latency;
		uint16_t timeout;	// [10ms]
	};
	BleStats getBleStats() const;

	static const char* bleReasonText(uint8_t reason);

	/**
	 * Small event log that survives crashes (RTC memory), stamped with local time, for
	 * diagnosing intermittent problems after the fact ("notifications stopped at 13:00").
	 */
	enum class Ev : uint8_t { Boot, LinkUp, LinkDown, Encrypted, AncsUp, AncsDown, NotifRx, WeatherRx, RegFail, DiscRetry, EncRetry, Sleep, Wake, SearchFail, AuthFail };
	void log(Ev type, uint16_t arg = 0);
	/** Calls fn(localEpochSeconds, type, arg) oldest first */
	void forEachEvent(const std::function<void(uint32_t, Ev, uint16_t)>& fn) const;
	static const char* evName(Ev type);

	/** Log level applied at every boot (persists). For debugging via the dev console. */
	void setBootLogLevel(uint8_t level);

	/** Internal RAM free / largest block at the last tick before this boot (0 = unknown) */
	uint32_t prevHeapFree = 0, prevHeapLargest = 0;

	/** Called periodically so a later crash can report how long the watch had been running */
	void tick();

private:
	nvs_handle_t handle{};
	BootLog bootLog;
	esp_reset_reason_t resetReason;

	std::atomic<uint32_t> connects{ 0 };
	std::atomic<uint32_t> disconnects{ 0 };
	std::atomic<uint8_t> lastReason{ 0 };
	std::atomic<uint64_t> lastDisconnectMs{ 0 };
	std::atomic<uint64_t> lastConnectMs{ 0 };
	std::atomic<bool> connected{ false };
	std::atomic<uint16_t> connInterval{ 0 }, connLatency{ 0 }, connTimeout{ 0 };

	mutable std::mutex logMut;

	static constexpr const char* NVSNamespace = "ArtemisDiag";
	static constexpr const char* BlobName = "BootLog";
};


#endif //ARTEMIS_FIRMWARE_DIAGNOSTICS_H
