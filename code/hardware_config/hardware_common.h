#pragma once

#include "state.h"
#include <SerialPortConstants.h>
#include <array>

namespace hardware
{
enum class LED
{
	LED_GPIO_STOP_SCAN,
	LED_GPIO_COPY_DATA,
	LED_GPIO_CONTINOUS_SCANNING,
	BUZZER,
};

enum class BUTTON
{
	BUTTON_STOP_SCAN,
	BUTTON_CONTINOUS_SCANNING
};
}; // namespace hardware

namespace GPIO
{
enum class GPIO_PULL
{
	UP,
	DOWN,
	OFF
};
};
