#pragma once

#include "gpios.h"
#include "hardware_common.h"
#include "utils/TimeStampProvider.h"
#ifdef MANDEYE_HARDWARE_CONFIGURED
#	error "MANDEYE Hardware were confiured. You included multiple hardware headers!"
#endif
#include <iostream>
#define MANDEYE_HARDWARE_CONFIGURED
namespace hardware
{
#define PISTACHE_SERVER
#define MANDEYE_BENCHMARK_WRITE_SPEED
#define MANDEYE_COUNTINOUS_SCANNING_STOP_1_CLICK

constexpr int Offset = 0;
constexpr bool Autostart = true;
constexpr const char* mandeyeHarwareType()
{
	return "MandeyeStandard";
}

constexpr const char* GetGPIOChip()
{
	//for raspberry pi 5
	return "/dev/gpiochip4";
}

inline void ReportState([[maybe_unused]] const mandeye::States state)
{
	if (state == mandeye::States::USB_IO_ERROR) {
		std::cerr << "USB IO ERROR - will restart shortly" << std::endl;
		static int count = 0;
		if (count ++ >= 2) {
			std::cerr << "Restarting..." << std::endl;
			std::abort();
		}
	}

}

inline void OnSavedLaz([[maybe_unused]] const std::string& filename)
{
	std::cout << "Saved LAZ file: " << filename << std::endl;
	if (mandeye::gpioClientPtr) {
		mandeye::gpioClientPtr->beep({200, 100, 200});
	}
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
		return 24;
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

[[maybe_unused]] inline const std::array<LED, 0> GetLidarSyncLEDs()
{
	return {};  // No hardware sync
}

[[maybe_unused]] inline  const std::array<const std::string, 0> GetLidarSyncPorts()
{
	return {}; // No hardware sync
};

[[maybe_unused]] inline const std::string GetGNSSPort()
{
	return "/dev/ttyAMA0";
};


[[maybe_unused]] inline const LibSerial::BaudRate GetGNSSBaudrate()
{
	return LibSerial::BaudRate::BAUD_115200;
};


} // namespace hardware
