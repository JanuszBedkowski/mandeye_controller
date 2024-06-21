#include <cppgpio.hpp>
#include <cppgpio/output.hpp>
#include <gpios.h>
#include <iostream>

namespace mandeye
{

std::unique_ptr<GPIO::DigitalOut> CreateDigitalOut(int pin)
{
	if (pin > 0)
	{
		return std::make_unique<GPIO::DigitalOut>(pin);
	}
	return nullptr;
}
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
		m_ledGpio[LED::LED_GPIO_STOP_SCAN] = CreateDigitalOut(hardware::GetLED(LED::LED_GPIO_STOP_SCAN));
		m_ledGpio[LED::LED_GPIO_COPY_DATA] = CreateDigitalOut(hardware::GetLED(LED::LED_GPIO_COPY_DATA));
		m_ledGpio[LED::LED_GPIO_CONTINOUS_SCANNING] = CreateDigitalOut(hardware::GetLED(LED::LED_GPIO_CONTINOUS_SCANNING));

		m_ledGpio[LED::BUZZER] = CreateDigitalOut(hardware::GetLED(LED::BUZZER));
		m_ledGpio[LED::LIDAR_SYNC_1] = CreateDigitalOut(hardware::GetLED(LED::LIDAR_SYNC_1));
		m_ledGpio[LED::LIDAR_SYNC_2] = CreateDigitalOut(hardware::GetLED(LED::LIDAR_SYNC_2));

		m_buttons[BUTTON::BUTTON_STOP_SCAN] =
			std::make_unique<PushButton>(hardware::GetButton(BUTTON::BUTTON_STOP_SCAN), hardware::GetPULL(BUTTON::BUTTON_STOP_SCAN));
		m_buttons[BUTTON::BUTTON_CONTINOUS_SCANNING] =
			std::make_unique<PushButton>(hardware::GetButton(BUTTON::BUTTON_CONTINOUS_SCANNING), hardware::GetPULL(BUTTON::BUTTON_STOP_SCAN));

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
				if (m_ledGpio[LED::BUZZER])
				{
					m_ledGpio[LED::BUZZER]->on();
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
					m_ledGpio[LED::BUZZER]->off();
				}
				std::cout << "Button :  " << ButtonName << std::endl;
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
	assert(m_ledGpio.find(led) != m_ledGpio.end());
	if (m_ledGpio[led] == nullptr)
	{
		std::cerr << "No LED with id " << (int)led << " in hardware config " << hardware::mandeyeHarwareType() << std::endl;
		return;
	}

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

void GpioClient::addButtonCallback(hardware::BUTTON btn,
								   const std::string& callbackName,
								   const std::function<void()>& callback)
{
	std::lock_guard<std::mutex> lck{m_lock};
	std::unordered_map<hardware::BUTTON, Callbacks>::iterator it = m_buttonsCallbacks.find(btn);
	if(it == m_buttonsCallbacks.end())
	{
		std::cerr << "No button with id " << (int)btn << std::endl;
		return;
	}
	(it->second)[callbackName] = callback;
}
void GpioClient::beep(const std::vector<int>& durations )
{
	std::lock_guard<std::mutex> lck{m_lock};
	if (m_ledGpio[LED::BUZZER] == nullptr)
	{
		std::cerr << "No LED with id " << (int)LED::BUZZER << " in hardware config " << hardware::mandeyeHarwareType() << std::endl;
		return;
	}
	if(!m_useSimulatedGPIO)
	{
		bool isOn = false;
		for(auto& duration : durations)
		{
			const auto sleepDuration = std::chrono::milliseconds(duration);
			if (!isOn)
			{
				m_ledGpio[LED::BUZZER]->on();
				std::this_thread::sleep_for(sleepDuration);
				isOn = true;
			}
			else
			{
				m_ledGpio[LED::BUZZER]->off();
				std::this_thread::sleep_for(sleepDuration);
				isOn = false;
			}
		}
		m_ledGpio[LED::BUZZER]->off();
	}
}


} // namespace mandeye