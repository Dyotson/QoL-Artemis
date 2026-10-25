#include "ExtSettings.h"
#include <esp_log.h>
#include <cstring>
#include <memory>

static const char* TAG = "ExtSettings";

ExtSettings::ExtSettings(){
	if(nvs_open(NVSNamespace, NVS_READWRITE, &handle) != ESP_OK){
		ESP_LOGE(TAG, "NVS open failed");
		return;
	}
	load();
}

ExtSettingsStruct ExtSettings::get(){
	std::lock_guard lock(mut);
	return data;
}

void ExtSettings::set(const ExtSettingsStruct& settings){
	std::lock_guard lock(mut);
	data = settings;
}

void ExtSettings::store(){
	std::lock_guard lock(mut);

	auto err = nvs_set_blob(handle, BlobName, &data, sizeof(data));
	if(err == ESP_OK){
		err = nvs_commit(handle);
	}
	if(err != ESP_OK){
		ESP_LOGW(TAG, "store error: %d", err);
	}
}

void ExtSettings::load(){
	size_t size = 0;
	if(nvs_get_blob(handle, BlobName, nullptr, &size) != ESP_OK || size == 0){
		ESP_LOGI(TAG, "No extended settings, using defaults");
		return;
	}

	// Blobs from older firmware are shorter: their bytes fill the prefix and the
	// appended fields keep their defaults. Newer (longer) blobs are truncated.
	std::unique_ptr<uint8_t[]> buf(new uint8_t[size]);
	if(nvs_get_blob(handle, BlobName, buf.get(), &size) != ESP_OK) return;

	memcpy(&data, buf.get(), std::min(size, sizeof(data)));

	// Defensive: keep strings terminated even if the blob got corrupted
	data.wifiSSID[sizeof(data.wifiSSID) - 1] = 0;
	data.wifiPass[sizeof(data.wifiPass) - 1] = 0;
	data.locationName[sizeof(data.locationName) - 1] = 0;
}
