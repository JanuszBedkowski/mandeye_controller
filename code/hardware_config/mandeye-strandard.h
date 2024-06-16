#pragma once

#include "hardware_common.h"
#ifdef MANDEYE_HARDWARE_CONFIGURED
#	error "MANDEYE Hardware were confiured. You included multiple hardware headers!"
#endif

#define MANDEYE_HARDWARE_CONFIGURED
namespace hardware
{

#define PISTACHE_SERVER
constexpr const char* mandeyeHarwareType()
{
	return "MandeyeStandard";
}

constexpr int GetLED(LED led)
{
	if(led == LED::LED_GPIO_STOP_SCAN)
	{
		return 26;
	}
	if(led == LED::LED_GPIO_COPY_DATA)
	{
		return 19;
	}
	if(led == LED::LED_GPIO_CONTINOUS_SCANNING)
	{
		return 13;
	}
	if(led == LED::LIDAR_SYNC_1)
	{
		return -1;
	}
	if(led == LED::LIDAR_SYNC_2)
	{
		return -1;
	}
	if(led == LED::BUZZER)
	{
		return -1;
	}
	return -1;
}
constexpr int GetButton(BUTTON btn)
{
	if(btn == BUTTON::BUTTON_STOP_SCAN)
	{
		return 5;
	}
	if(btn == BUTTON::BUTTON_CONTINOUS_SCANNING)
	{
		return 6;
	}
	return -1;
}

constexpr GPIO::GPIO_PULL GetPULL([[maybe_unused]] BUTTON btn)
{
	return GPIO::GPIO_PULL::UP;
}

[[maybe_unused]] const std::string GetGNSSPort()
{
	return "/dev/ttyS0";
};

[[maybe_unused]] const LibSerial::BaudRate GetGNSSBaudrate()
{
	return LibSerial::BaudRate::BAUD_9600;
};
} // namespace hardware
