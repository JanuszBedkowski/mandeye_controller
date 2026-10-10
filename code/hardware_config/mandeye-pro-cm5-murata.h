#pragma once

// mandeye-pro-cm5-murata rev110: Raspberry Pi CM5 carrier with an STM32H7 timing MCU.
//
// CM5 GPIO use on this board (do not reuse for anything else):
//   GPIO4/5   uart2 (/dev/ttyAMA2) link to the STM32 (IMU stream, 921600)
//   GPIO7     FAKE_PPS_OUT  -> STM32 PPS input
//   GPIO12/13 uart4 (/dev/ttyAMA4) FakeNMEA -> STM32 (GPIO13 is RX: never drive)
//   GPIO19    STM32_BOOT (BOOT0): must stay low, or the STM32 boots its ROM bootloader
//   GPIO21    STM32_RST (NRST)
// config.txt needs dtoverlay=uart2-pi5 and dtoverlay=uart4-pi5.

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
constexpr bool Autostart = false;
constexpr bool WaitForLidarSync = true;

constexpr const char* mandeyeHarwareType()
{
	return "MandeyeStandard";
}

constexpr const char* GetGPIOChip()
{
	// RP1 on CM5; kernels from 6.6.x late 2024 on name it /dev/gpiochip0
	return "/dev/gpiochip4";
}

inline void ReportState([[maybe_unused]] const mandeye::States state)
{
	if(state == mandeye::States::USB_IO_ERROR)
	{
		std::cerr << "USB IO ERROR - will restart shortly" << std::endl;
		static int count = 0;
		if(count++ >= 2)
		{
			std::cerr << "Restarting..." << std::endl;
			std::abort();
		}
	}
}

inline void OnSavedLaz([[maybe_unused]] const std::string& filename)
{
	std::cout << "Saved LAZ file: " << filename << std::endl;
	if(mandeye::gpioClientPtr)
	{
		mandeye::gpioClientPtr->beep({50, 100, 50, 100});
	}
}

constexpr int GetLED(LED led)
{
	if(led == LED::LED_GPIO_STOP_SCAN)
	{
		return 26; // LED_STOP/SCAN
	}
	if(led == LED::LED_GPIO_COPY_DATA)
	{
		return 24; // LED_COPYDATA
	}
	if(led == LED::LED_GPIO_CONTINOUS_SCANNING)
	{
		return 20; // LED_CONTINOUS
	}
	if(led == LED::BUZZER)
	{
		return 16; // BUZZER (Q1)
	}
	return -1;
}

constexpr int GetButton(BUTTON btn)
{
	// Keyboard (J11): buttons switch to 3V3 with 1k pull-downs (R32/R33).
	// GPIO3 also has the CM5's 1.8k I2C pull-up, which fights R33 on rev110.
	if(btn == BUTTON::BUTTON_STOP_SCAN)
	{
		return 3; // BTN_STOP
	}
	if(btn == BUTTON::BUTTON_CONTINOUS_SCANNING)
	{
		return 18; // BTN_WALK
	}
	return -1;
}

constexpr GPIO::GPIO_PULL GetPULL([[maybe_unused]] BUTTON btn)
{
	return GPIO::GPIO_PULL::DOWN; // active high
}

[[maybe_unused]] inline const std::array<int, 1> GetLidarSyncGPIO()
{
	return {7}; // FAKE_PPS_OUT -> STM32
}

[[maybe_unused]] inline const std::array<const std::string, 1> GetLidarSyncPorts()
{
	return {"/dev/ttyAMA4"}; // FakeNMEA -> STM32 (uart4, GPIO12)
};

[[maybe_unused]] inline const std::string GetGNSSPort()
{
	// The ZED-X20P's UART shares the STM32 link wires on rev110 and is disabled,
	// so there is no GNSS serial port.
	return "";
};

[[maybe_unused]] inline const LibSerial::BaudRate GetGNSSBaudrate()
{
	return LibSerial::BaudRate::BAUD_38400;
};

} // namespace hardware
