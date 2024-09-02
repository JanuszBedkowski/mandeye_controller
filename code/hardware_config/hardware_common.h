#pragma once
#include <SerialPortConstants.h>
namespace hardware
{
	enum class LED
	{
		LED_GPIO_STOP_SCAN,
		LED_GPIO_COPY_DATA,
		LED_GPIO_CONTINOUS_SCANNING,
		BUZZER,
		LIDAR_SYNC_1,
		LIDAR_SYNC_2,
	};

	enum class BUTTON
	{
		BUTTON_STOP_SCAN,
		BUTTON_CONTINOUS_SCANNING
	};
};

namespace GPIO
{
	enum class GPIO_PULL
	{
		UP,
		DOWN,
		OFF
	};
};
