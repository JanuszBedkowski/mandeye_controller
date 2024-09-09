#pragma once

#include <json.hpp>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include "hardware_config/mandeye.h"

// Forward declaration for GPIOD
struct gpiod_chip;
struct gpiod_line;

namespace mandeye
{
using namespace hardware;
// forward declaration of cppgpio type that I want to keep inside compliation unit


class GpioClient
{
	using Callbacks = std::unordered_map<std::string, std::function<void()>>;

struct ButtonData{
	std::string m_name; //! button name
	int m_pin; //! pin number
	gpiod_line *m_line{nullptr};
	uint32_t m_pressedTime {0}; //! time when button was pressed
	GPIO::GPIO_PULL m_pullMode; //! pull up or down
	bool m_pressed; //! is button pressed
	Callbacks m_callbacks; //! callbacks to call when button is pressed
	static constexpr int DEBOUNCE_TIME = 2; //! debounce time in cycles

	static bool GetButtonState(const ButtonData& button);
	void CallUserCallbacks();
};

struct LedData{
	std::string m_name;
	bool m_state{false};
	int m_pin;
	gpiod_line *m_line{nullptr};
};

public:
	//! Constructor
	//! @param sim if true hardware is not called
	GpioClient(bool sim);

	//! Destructor
	~GpioClient();

	GpioClient(const GpioClient&) = delete;
	GpioClient& operator=(const GpioClient&) = delete;

	//! serialize component state to API
	nlohmann::json produceStatus();

	//! set led to given state
	void setLed(hardware::LED led, bool state);
	void setLed(LedData& led, bool state);


	//! addcalback
	void addButtonCallback(hardware::BUTTON btn,
						   const std::string& callbackName,
						   const std::function<void()>& callback);

	//! allow to beep the buzzer with a given duration
	void beep(const std::vector<int> &durations );
private:
	std::thread  m_gpioReadBackThread;
	gpiod_chip *m_chip{nullptr}; //!< GPIO chip

	//! use simulated GPIOs instead real one
	bool m_useSimulatedGPIO{true};

	//! available LEDs
	std::unordered_map<hardware::LED, LedData> m_ledGpio;

	//! available Buttons
	std::unordered_map<hardware::BUTTON, ButtonData> m_buttons;


	//! useful translations
	const std::unordered_map<LED, std::string> LedToName{
		{LED::LED_GPIO_STOP_SCAN, "LED_GPIO_STOP_SCAN"},
		{LED::LED_GPIO_COPY_DATA, "LED_GPIO_COPY_DATA"},
		{LED::LED_GPIO_CONTINOUS_SCANNING, "LED_GPIO_CONTINOUS_SCANNING"},
		{LED::BUZZER, "BUZZER"},
	};

	const std::unordered_map<std::string, LED> NameToLed{
		{"LED_GPIO_STOP_SCAN", LED::LED_GPIO_STOP_SCAN},
		{"LED_GPIO_COPY_DATA", LED::LED_GPIO_COPY_DATA},
		{"LED_GPIO_CONTINOUS_SCANNING", LED::LED_GPIO_CONTINOUS_SCANNING},
		{"BUZZER", LED::BUZZER},
		{"LIDAR_SYNC_1", LED::LIDAR_SYNC_1},
		{"LIDAR_SYNC_2", LED::LIDAR_SYNC_2},
	};

	const std::unordered_map<BUTTON, std::string> ButtonToName{
		{BUTTON::BUTTON_STOP_SCAN, "BUTTON_STOP_SCAN"},
		{BUTTON::BUTTON_CONTINOUS_SCANNING, "BUTTON_CONTINOUS_SCANNING"},
	};

	const std::unordered_map<std::string, BUTTON> NameToButton{
		{"BUTTON_STOP_SCAN", BUTTON::BUTTON_STOP_SCAN},
		{"BUTTON_CONTINOUS_SCANNING", BUTTON::BUTTON_CONTINOUS_SCANNING},
	};

	std::mutex m_lock;
	std::atomic<bool> m_running{true};
};
} // namespace mandeye