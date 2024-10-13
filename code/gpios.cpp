#include <fstream>
#include <gpios.h>
#include <iostream>
#include <gpiod.h>

namespace mandeye
{
using namespace hardware;
using namespace GPIO;
bool GpioClient::ButtonData::GetButtonState(const GpioClient::ButtonData& button)
{
	if (button.m_line == nullptr)
	{
		return false;
	}
	int rawButtonState = gpiod_line_get_value(button.m_line);
	if(button.m_pullMode == GPIO::GPIO_PULL::UP)
	{
		return rawButtonState == 0;
	}
	else
	{
		return rawButtonState == 1;
	}
}
void GpioClient::ButtonData::CallUserCallbacks()
{
	for(auto& [name, callback] : m_callbacks)
	{
		callback();
	}
}

GpioClient::GpioClient(bool sim)
	: m_useSimulatedGPIO(sim)
{

	using namespace GPIO;
	for(auto& [id, name] : ButtonToName)
	{
		m_buttons[id].m_name = name;
	}
	for (auto& [led, name] : LedToName)
	{
		m_ledGpio[led].m_name = name;
	}

	if(!sim)
	{
		const auto& chipPath = mandeye::GetGPIOChip();
		std::cout << "Opening GPIO chip " << chipPath << std::endl;

		m_chip = gpiod_chip_open(chipPath);
		if (!m_chip) {
			std::cerr << "Error: Unable to open GPIO chip." << std::endl;
			return;
		}
		std::lock_guard<std::mutex> lck{m_lock};

		// initialize leds
		for (auto &[led, name] : LedToName)
		{
			LedData& ledData = m_ledGpio[led];
			ledData.m_name = name;
			ledData.m_pin = hardware::GetLED(led);
			if(ledData.m_pin == -1)
			{
				std::cerr << "No LED with id " << name << " in hardware config " << hardware::mandeyeHarwareType() << std::endl;
			}
			else
			{
				ledData.m_line = gpiod_chip_get_line(m_chip, ledData.m_pin);

				if(!ledData.m_line)
				{
					std::cerr << "Failed to create line at pin " << ledData.m_pin << " of " << ledData.m_name << std::endl;
					continue ;
				}
				int ret = gpiod_line_request_output(ledData.m_line, ledData.m_name.c_str(), 0);
				if(ret < 0)
				{
					std::cerr << "Failed to create line at pin " << ledData.m_pin << " of " << ledData.m_name << std::endl;
					continue ;
				}
			}
		}
		std::cout<< "LEDs initialized" << std::endl;


		for(auto& [button, name] : ButtonToName)
		{
			ButtonData& buttonData = m_buttons[button];
			buttonData.m_pin = hardware::GetButton(button);
			buttonData.m_pullMode = hardware::GetPULL(button);
			if(buttonData.m_pin == -1)
			{
				std::cerr << "No button with id " <<  name << " in hardware config " << hardware::mandeyeHarwareType() << std::endl;
			}
			else
			{
				buttonData.m_line = gpiod_chip_get_line(m_chip, buttonData.m_pin);
				if(!buttonData.m_line)
				{
					std::cerr << "Failed to create line at pin " << buttonData.m_pin << " of " << buttonData.m_name << std::endl;
					continue;
				}
				int flags = 0;
				if(buttonData.m_pullMode == GPIO::GPIO_PULL::UP)
				{
					flags = GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP;
				}
				else
				{
					flags = GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_DOWN;
				}
				int ret = gpiod_line_request_input_flags(buttonData.m_line, buttonData.m_name.c_str(), flags);
				if(ret < 0)
				{
					std::cerr << "Failed to create line at pin " << buttonData.m_pin << " of " << buttonData.m_name << std::endl;
					continue ;
				}

			}

		}
	}


	for(auto& [buttonID, ButtonName] : ButtonToName)
	{
		{
			addButtonCallback(buttonID, "DBG" + ButtonName, [this, ButtonName]() {
				auto buzzerGPIO = m_ledGpio[LED::BUZZER];
				setLed(buzzerGPIO, true);
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				setLed(buzzerGPIO, false);
				std::cout << "Button :  " << ButtonName << std::endl;
			});
		}
	};

	m_gpioReadBackThread = std::thread([this]() {
		while(m_running)
		{
			for(auto& [buttonID, buttonData] : m_buttons)
			{
				bool rawButtonState = ButtonData::GetButtonState(buttonData);
				if(rawButtonState == true)
				{
					buttonData.m_pressedTime++;
					if(buttonData.m_pressedTime == buttonData.DEBOUNCE_TIME)
					{
						buttonData.m_pressed = true;
						buttonData.CallUserCallbacks();
					}
				}
				else
				{
					buttonData.m_pressedTime = 0;
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(25));
		}
	});
}
GpioClient::~GpioClient()
{
	if (m_gpioReadBackThread.joinable())
	{
		m_running.store(false);
		m_gpioReadBackThread.join();
	}
	if(m_chip)
	{
		for(auto& [led, ledData] : m_ledGpio)
		{
			if(ledData.m_line)
			{
				gpiod_line_release(ledData.m_line);
			}
		}
		for(auto& [button, buttonData] : m_buttons)
		{
			if(buttonData.m_line)
			{
				gpiod_line_release(buttonData.m_line);
			}
		}
		gpiod_chip_close(m_chip);
	}

}

nlohmann::json GpioClient::produceStatus()
{
	nlohmann::json data;
	std::lock_guard<std::mutex> lck{m_lock};
	for(const auto& [ledid, ledname] : LedToName)
	{
		try
		{
			data["leds"][ledname] = m_ledGpio.at(ledid).m_state;
		}
		catch(const std::out_of_range& ex)
		{
			data["leds"][ledname] = "NA";
		}
	}

	for(const auto& [buttonid, buttonname] : ButtonToName)
	{

		auto it = m_buttons.find(buttonid);
		if(it != m_buttons.end())
		{
			data["buttons"][buttonname] = it->second.m_pressed;
		}
		else
		{
			data["buttons"][buttonname] = "NA";
		}
	}
	return data;
}

void GpioClient::setLed(LED led, bool state)
{
	assert(m_ledGpio.find(led) != m_ledGpio.end());
	setLed(m_ledGpio[led], state);
}

void GpioClient::setLed(LedData& led, bool state)
{
	std::lock_guard<std::mutex> lck{m_lock};
	led.m_state = state;
	if(!m_useSimulatedGPIO && led.m_line)
	{
		gpiod_line_set_value(led.m_line, state?1:0);
	}
}

void GpioClient::addButtonCallback(hardware::BUTTON btn, const std::string& callbackName, const std::function<void()>& callback)
{
	std::lock_guard<std::mutex> lck{m_lock};
	auto it = m_buttons.find(btn);

	if(it == m_buttons.end())
	{
		std::cerr << "No button with id " << (int)btn << std::endl;
		return;
	}
	it->second.m_callbacks[callbackName] = callback;
}

void GpioClient::beep(const std::vector<int>& durations)
{
	//std::lock_guard<std::mutex> lck{m_lock};

	if(!m_useSimulatedGPIO)
	{
		bool isOn = false;
		auto& buzzerGpio = m_ledGpio[LED::BUZZER];
		for(auto& duration : durations)
		{
			const auto sleepDuration = std::chrono::milliseconds(duration);
			if(!isOn)
			{
				setLed(buzzerGpio, true);
				std::this_thread::sleep_for(sleepDuration);
				isOn = true;
			}
			else
			{
				setLed(buzzerGpio, false);
				std::this_thread::sleep_for(sleepDuration);
				isOn = false;
			}
		}
	}
}

} // namespace mandeye
