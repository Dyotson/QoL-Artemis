#include "Diagnostics.h"
#include "Util/stdafx.h"
#include <esp_attr.h>
#include <esp_core_dump.h>
#include <esp_log.h>
#include <cstring>
#include <cstdio>

static const char* TAG = "Diag";

// Survives software resets, panics and watchdog resets (but not power loss), so the
// next boot can tell how long the previous one ran before it went down.
static constexpr uint32_t RtcMagic = 0xA27E3150;
RTC_NOINIT_ATTR static uint32_t rtcMagic;
RTC_NOINIT_ATTR static uint32_t rtcUptime;

Diagnostics::Diagnostics() : resetReason(esp_reset_reason()){
	const uint32_t prevUptime = (rtcMagic == RtcMagic) ? rtcUptime : 0;
	rtcMagic = RtcMagic;
	rtcUptime = 0;

	if(nvs_open(NVSNamespace, NVS_READWRITE, &handle) != ESP_OK){
		ESP_LOGE(TAG, "NVS open failed");
		return;
	}

	size_t size = sizeof(bootLog);
	if(nvs_get_blob(handle, BlobName, &bootLog, &size) != ESP_OK || size != sizeof(bootLog)){
		bootLog = {};
	}

	memmove(&bootLog.boots[1], &bootLog.boots[0], sizeof(Boot) * (BootHistory - 1));
	bootLog.boots[0] = { .reason = (uint8_t) resetReason, .prevUptime = prevUptime };
	if(bootLog.count < BootHistory) bootLog.count++;
	bootLog.bootCount++;

	nvs_set_blob(handle, BlobName, &bootLog, sizeof(bootLog));
	nvs_commit(handle);

	uint8_t logLevel;
	if(nvs_get_u8(handle, "logLevel", &logLevel) == ESP_OK && logLevel <= ESP_LOG_VERBOSE){
		esp_log_level_set("*", (esp_log_level_t) logLevel);
	}

	printf("Boot #%lu, reset reason: %s, previous uptime: %lus\n", bootLog.bootCount, reasonText(resetReason), prevUptime);
	if(hasCoredump()){
		printf("Stored crash: %s\n", coredumpSummary().c_str());
	}
}

const Diagnostics::BootLog& Diagnostics::getBootLog() const{
	return bootLog;
}

esp_reset_reason_t Diagnostics::getResetReason() const{
	return resetReason;
}

const char* Diagnostics::reasonText(uint8_t reason){
	switch((esp_reset_reason_t) reason){
		case ESP_RST_POWERON: return "Power on";
		case ESP_RST_EXT: return "External pin";
		case ESP_RST_SW: return "Software restart";
		case ESP_RST_PANIC: return "Crash (panic)";
		case ESP_RST_INT_WDT: return "Interrupt watchdog";
		case ESP_RST_TASK_WDT: return "Task watchdog";
		case ESP_RST_WDT: return "Other watchdog";
		case ESP_RST_DEEPSLEEP: return "Deep sleep wake";
		case ESP_RST_BROWNOUT: return "Brownout (low voltage)";
		case ESP_RST_SDIO: return "SDIO";
		case ESP_RST_USB: return "USB reset";
		default: return "Unknown";
	}
}

bool Diagnostics::reasonIsCrash(uint8_t reason){
	switch((esp_reset_reason_t) reason){
		case ESP_RST_PANIC:
		case ESP_RST_INT_WDT:
		case ESP_RST_TASK_WDT:
		case ESP_RST_WDT:
		case ESP_RST_BROWNOUT:
			return true;
		default:
			return false;
	}
}

bool Diagnostics::hasCoredump() const{
	size_t addr, size;
	return esp_core_dump_image_get(&addr, &size) == ESP_OK && size > 0;
}

std::string Diagnostics::coredumpSummary() const{
	if(!hasCoredump()) return {};

	auto summary = (esp_core_dump_summary_t*) malloc(sizeof(esp_core_dump_summary_t));
	if(summary == nullptr) return "present";

	std::string out = "present";
	if(esp_core_dump_get_summary(summary) == ESP_OK){
		char buf[64];
		snprintf(buf, sizeof(buf), "%.16s @ 0x%08lx", summary->exc_task, summary->exc_pc);
		out = buf;
	}

	free(summary);
	return out;
}

void Diagnostics::clearCoredump(){
	esp_core_dump_image_erase();
}

void Diagnostics::onBleConnect(){
	connects++;
	lastConnectMs = millis();
	connected = true;
}

void Diagnostics::onBleDisconnect(uint8_t reason){
	disconnects++;
	lastReason = reason;
	lastDisconnectMs = millis();
	connected = false;
}

void Diagnostics::onBleConnParams(uint16_t interval, uint16_t latency, uint16_t timeout){
	connInterval = interval;
	connLatency = latency;
	connTimeout = timeout;
}

Diagnostics::BleStats Diagnostics::getBleStats() const{
	return {
			.connects = connects,
			.disconnects = disconnects,
			.lastReason = lastReason,
			.lastDisconnectMs = lastDisconnectMs,
			.lastConnectMs = lastConnectMs,
			.connected = connected,
			.interval = connInterval,
			.latency = connLatency,
			.timeout = connTimeout
	};
}

const char* Diagnostics::bleReasonText(uint8_t reason){
	switch(reason){
		case 0x08: return "Link timeout";
		case 0x13: return "Phone closed link";
		case 0x16: return "Watch closed link";
		case 0x3e: return "Failed to establish";
		case 0x22: return "LMP timeout";
		case 0x3d: return "MIC failure";
		case 0x05: return "Auth failure";
		case 0x00: return "-";
		default: return "Other";
	}
}

void Diagnostics::tick(){
	rtcUptime = (uint32_t) (millis() / 1000);
}

void Diagnostics::setBootLogLevel(uint8_t level){
	nvs_set_u8(handle, "logLevel", level);
	nvs_commit(handle);
}
