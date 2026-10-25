#ifndef CLOCKSTAR_FIRMWARE_SERVICES_H
#define CLOCKSTAR_FIRMWARE_SERVICES_H

#include <unordered_map>

enum class Service { IMU, Phone, Time, Audio, Settings, Sleep, Battery, Backlight, Status, Input, LVGL, Diag, ExtSettings, Net, Weather, Clocks, Activity, Thermo, Melody };

class ServiceLocator {
public:
	void set(Service service, void* ptr);
	void* get(Service service);

private:
	std::unordered_map<Service, void*> services;


};

extern ServiceLocator Services;

#endif //CLOCKSTAR_FIRMWARE_SERVICES_H
