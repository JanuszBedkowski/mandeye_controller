#pragma once

#include "hardware_common.h"
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

/***
 Mandeye standard hardware configuration for Raspberry Pi 5.

 Changes to `/boot/firmware/config.txt`:
 ```
# External access via debug probe:
enable_uart=1
console=serial0,115200
dtoverlay=disable-bt

# Enable /dev/ttyAMA0 for GNSS, connected to GPIO14 and 15, needs
dtparam=uart0 #TX is GPIO14(pin8) and RX is GPIO15(pin10)

# Standard camera
camera_auto_detect=0
dtoverlay=imx519,cam0

# Cameras for dog system
#camera_auto_detect=0
#dtoverlay=arducam-pivariety,cam1
#dtoverlay=ov9281,cam0


# PPS Listen GPIO18 (pin12), PPS simulation pin GPIO17 (pin11) and uses /dev/ttyAMA1 to send fake pps.
dtoverlay=pps-gpio,gpiopin=18
#TX is GPIO0(pin27) and RX is GPIO1(pin28)
dtparam=uart1
dtoverlay=uart1
````

and changes to `/boot/firmware/cmdline.txt`:
```
console=serial0,115200
```
 * ***/
constexpr int Offset = 0;
constexpr bool Autostart = false;
constexpr bool WaitForLidarSync = false;
constexpr const char* mandeyeHarwareType()
{
	return "MandeyeStandard";
}

constexpr const char* GetGPIOChip()
{
	//for raspberry pi 5
	return "/dev/gpiochip4";
}

inline void ReportState([[maybe_unused]] const mandeye::States state) { }

inline void OnSavedLaz([[maybe_unused]] const std::string& filename) { }

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
	if(led == LED::BUZZER)
	{
		return 12;
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

[[maybe_unused]] inline const std::array<int, 1> GetLidarSyncGPIO()
{
	return {17}; // GPIO 17 (pin11) is used to send fake
}

[[maybe_unused]] inline const std::array<const std::string, 1> GetLidarSyncPorts()
{
	return {"/dev/ttyAMA1"};
};

[[maybe_unused]] inline const std::string GetGNSSPort()
{
	return "";
};

[[maybe_unused]] inline const LibSerial::BaudRate GetGNSSBaudrate()
{
	return LibSerial::BaudRate::BAUD_115200;
};

} // namespace hardware
