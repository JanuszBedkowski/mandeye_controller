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
	return "MandeyePro";
}

constexpr const char* GetGPIOChip()
{
	return "/dev/gpiochip0";
}


constexpr int GetLED(LED led)
{
	if(led == LED::LED_GPIO_STOP_SCAN)
	{
		return 20;
	}
	if(led == LED::LED_GPIO_COPY_DATA)
	{
		return 16;
	}
	if(led == LED::LED_GPIO_CONTINOUS_SCANNING)
	{
		return 21;
	}
	if(led == LED::LIDAR_SYNC_1)
	{
		return 3;
	}
	if(led == LED::LIDAR_SYNC_2)
	{
		return 2;
	}
	if(led == LED::BUZZER)
	{
		return 24;
	}
	return -1;
}
constexpr int GetButton(BUTTON btn)
{
	if(btn == BUTTON::BUTTON_STOP_SCAN)
	{
		return 25;
	}
	if(btn == BUTTON::BUTTON_CONTINOUS_SCANNING)
	{
		return 8;
	}
	return -1;
}

constexpr GPIO::GPIO_PULL GetPULL([[maybe_unused]] BUTTON btn)
{
	return GPIO::GPIO_PULL::DOWN;
}

[[maybe_unused]] inline const std::array<LED, 2> GetLidarSyncLEDs()
{
	return {LED::LIDAR_SYNC_1, LED::LIDAR_SYNC_2};
}

[[maybe_unused]] inline  const std::array<const std::string, 2> GetLidarSyncPorts()
{
	return {"/dev/ttyAMA1", // GPIO ID_SD (TXD2)
			"/dev/ttyAMA4"}; // GPIO 12 (TXD5)
};

[[maybe_unused]] inline const std::string GetGNSSPort()
{
	return "/dev/ttyAMA2";
};

[[maybe_unused]] inline const LibSerial::BaudRate GetGNSSBaudrate()
{
	return LibSerial::BaudRate::BAUD_38400;
};

} // namespace hardware
