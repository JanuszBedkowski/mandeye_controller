#include <cppgpio.hpp>
#include <cppgpio/output.hpp>
#include <gpios.h>
#include <iostream>

namespace mandeye
{

GpioClient::GpioClient(bool sim)
	: m_useSimulatedGPIO(sim)
{

	using namespace GPIO;
	for(auto& [id, name] : ButtonToName)
	{
		m_buttonsCallbacks[id] = Callbacks{};
	}

	if(!sim)
	{
		std::lock_guard<std::mutex> lck{m_lock};
		m_ledGpio[LED::LED_GPIO_RED] = std::make_unique<DigitalOut>(26);
		m_ledGpio[LED::LED_GPIO_GREEN] = std::make_unique<DigitalOut>(19);
		m_ledGpio[LED::LED_GPIO_BLUE] = std::make_unique<DigitalOut>(13);
		m_ledGpio[LED::LED_GPIO_YELLOW] = std::make_unique<DigitalOut>(6);
		
		m_buttons[BUTTON::BUTTON_1] = std::make_unique<PushButton>(0, GPIO::GPIO_PULL::UP);
		m_buttons[BUTTON::BUTTON_2] = std::make_unique<PushButton>(5, GPIO::GPIO_PULL::UP);

		for(auto& [buttonID, ptr] : m_buttons)
		{
			ptr->start();
			ptr->f_pushed = [&]() {
				auto& it = m_buttonsCallbacks[buttonID];
				for(auto& [name, callback] : it)
				{
					callback();
				}
			};
		}
	}

	for(auto& [buttonID, ButtonName] : ButtonToName)
	{
		{
			addButtonCallback(buttonID, "DBG" + ButtonName, [&]() {
				std::cout << "TestButton " << ButtonName << std::endl;
			});
		}
	};
}

nlohmann::json GpioClient::produceStatus()
{
	nlohmann::json data;
	std::lock_guard<std::mutex> lck{m_lock};
	for(const auto& [ledid, ledname] : LedToName)
	{
		try
		{
			data["leds"][ledname] = m_ledState.at(ledid);
		}
		catch(const std::out_of_range& ex)
		{
			data["leds"][ledname] = "NA";
		}
	}

	for(const auto& [buttonid, buttonname] : ButtonToName)
	{
		try
		{
			data["buttons"][buttonname] = m_buttons.at(buttonid)->is_on();
		}
		catch(const std::out_of_range& ex)
		{
			data["buttons"][buttonname] = "NA";
		}
	}
	return data;
}

void GpioClient::setLed(LED led, bool state)
{
	std::lock_guard<std::mutex> lck{m_lock};
	m_ledState[led] = state;
	if(!m_useSimulatedGPIO)
	{
		if(state)
		{
			m_ledGpio[led]->on();
		}
		else
		{
			m_ledGpio[led]->off();
		}
	}
}

void GpioClient::addButtonCallback(GpioClient::BUTTON btn,
								   const std::string& callbackName,
								   const std::function<void()>& callback)
{
	std::lock_guard<std::mutex> lck{m_lock};
	std::unordered_map<GpioClient::BUTTON, Callbacks>::iterator it = m_buttonsCallbacks.find(btn);
	if(it == m_buttonsCallbacks.end())
	{
		std::cerr << "No button with id " << (int)btn << std::endl;
		return;
	}
	(it->second)[callbackName] = callback;
}

} // namespace mandeye