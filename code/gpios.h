#pragma once
#include <cppgpio/buttons.hpp>
#include <cppgpio/output.hpp>
#include <json.hpp>
#include <mutex>
#include <unordered_map>
namespace mandeye
{

// forward declaration of cppgpio type that I want to keep inside compliation unit

class GpioClient
{
public:
	enum class LED
	{
		LED_GPIO_STOP_SCAN,
		LED_GPIO_COPY_DATA,
		LED_GPIO_CONTINOUS_SCANNING
	};

	enum class BUTTON
	{
		BUTTON_STOP_SCAN,
		BUTTON_CONTINOUS_SCANNING
	};

public:
	//! Constructor
	//! @param sim if true hardware is not called
	GpioClient(bool sim);

	//! serialize component state to API
	nlohmann::json produceStatus();

	//! set led to given state
	void setLed(LED led, bool state);

	//! addcalback
	void addButtonCallback(BUTTON btn,
						   const std::string& callbackName,
						   const std::function<void()>& callback);

private:
	using Callbacks = std::unordered_map<std::string, std::function<void()>>;

	//! use simulated GPIOs instead real one
	bool m_useSimulatedGPIO{true};

	std::unordered_map<LED, bool> m_ledState{
		{LED::LED_GPIO_STOP_SCAN, false},
		{LED::LED_GPIO_COPY_DATA, false},
		{LED::LED_GPIO_CONTINOUS_SCANNING, false},
		
	};

	//! available LEDs
	std::unordered_map<GpioClient::LED, std::unique_ptr<GPIO::DigitalOut>> m_ledGpio;

	//! available Buttons
	std::unordered_map<GpioClient::BUTTON, std::unique_ptr<GPIO::PushButton>> m_buttons;

	std::unordered_map<GpioClient::BUTTON, Callbacks> m_buttonsCallbacks;

	//! useful translations
	const std::unordered_map<LED, std::string> LedToName{
		{LED::LED_GPIO_STOP_SCAN, "LED_GPIO_STOP_SCAN"},
		{LED::LED_GPIO_COPY_DATA, "LED_GPIO_COPY_DATA"},
		{LED::LED_GPIO_CONTINOUS_SCANNING, "LED_GPIO_CONTINOUS_SCANNING"},
	};

	const std::unordered_map<std::string, LED> NameToLed{
		{"LED_GPIO_STOP_SCAN", LED::LED_GPIO_STOP_SCAN},
		{"LED_GPIO_COPY_DATA", LED::LED_GPIO_COPY_DATA},
		{"LED_GPIO_CONTINOUS_SCANNING", LED::LED_GPIO_CONTINOUS_SCANNING},
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
};
} // namespace mandeye