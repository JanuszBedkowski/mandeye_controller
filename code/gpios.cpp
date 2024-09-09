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
	int rawButtonState = -1;
	//GetDigitalIn(button.m_pin, rawButtonState);
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

		const std::vector<LED> leds {
			LED::LED_GPIO_STOP_SCAN,
			LED::LED_GPIO_COPY_DATA,
			LED::LED_GPIO_CONTINOUS_SCANNING,
			LED::BUZZER,
			LED::LIDAR_SYNC_1,
			LED::LIDAR_SYNC_2,
		};
		for (auto &led : leds)
		{
			LedData ledData;
			ledData.m_name = LedToName.at(led);
			ledData.m_pin = hardware::GetLED(led);
			ledData.m_line = gpiod_chip_get_line(m_chip, ledData.m_pin);
			if (!ledData.m_line)
			{
				std::cerr << "Failed to create line at pin " << ledData.m_pin << " of " << ledData.m_name << std::endl;
			}
			int ret = gpiod_line_request_output(ledData.m_line, ledData.m_name.c_str(), 0);
			if (ret < 0)
			{
				std::cerr << "Failed to create line at pin " << ledData.m_pin << " of " << ledData.m_name << std::endl;
			}
			m_ledGpio[led] = ledData;
		}

		// set pull

//		CreateDigitalIn(hardware::GetButton(BUTTON::BUTTON_STOP_SCAN));
//		CreateDigitalIn(hardware::GetButton(BUTTON::BUTTON_CONTINOUS_SCANNING));
//
//		m_buttons[BUTTON::BUTTON_STOP_SCAN].m_pin = hardware::GetButton(BUTTON::BUTTON_STOP_SCAN);
//		m_buttons[BUTTON::BUTTON_CONTINOUS_SCANNING].m_pin = hardware::GetButton(BUTTON::BUTTON_CONTINOUS_SCANNING);
//
//		m_buttons[BUTTON::BUTTON_STOP_SCAN].m_pullMode = hardware::GetPULL(BUTTON::BUTTON_STOP_SCAN);
//		m_buttons[BUTTON::BUTTON_CONTINOUS_SCANNING].m_pullMode = hardware::GetPULL(BUTTON::BUTTON_CONTINOUS_SCANNING);
//
//		// set pull
//		for(auto& [_, button] : m_buttons)
//		{
//			setPullUp(button.m_pin, button.m_pullMode);
//		}

		//		m_buttons[BUTTON::BUTTON_STOP_SCAN] =
		//			std::make_unique<PushButton>(hardware::GetButton(BUTTON::BUTTON_STOP_SCAN), hardware::GetPULL(BUTTON::BUTTON_STOP_SCAN));
		//		m_buttons[BUTTON::BUTTON_CONTINOUS_SCANNING] =
		//			std::make_unique<PushButton>(hardware::GetButton(BUTTON::BUTTON_CONTINOUS_SCANNING), hardware::GetPULL(BUTTON::BUTTON_STOP_SCAN));
	}

	for(auto& [buttonID, ButtonName] : ButtonToName)
	{
		{
			addButtonCallback(buttonID, "DBG" + ButtonName, [&]() {
				//				if (mandeye::hardware::Ge
				//				{
				//					m_ledGpio[LED::BUZZER]->on();
				//					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				//					m_ledGpio[LED::BUZZER]->off();
				//				}
				std::cout << "Button :  " << ButtonName << std::endl;
			});
		}
	};

	m_gpioReadBackThread = std::thread([this]() {
		while(true)
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
	std::lock_guard<std::mutex> lck{m_lock};
	m_ledState[led] = state;
	if(!m_useSimulatedGPIO)
	{
		gpiod_line_set_value(m_ledGpio[led].m_line, state?0:1);
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
	std::lock_guard<std::mutex> lck{m_lock};

	std::cerr << "No LED with id " << (int)LED::BUZZER << " in hardware config " << hardware::mandeyeHarwareType() << std::endl;
	return;

	if(!m_useSimulatedGPIO)
	{
		bool isOn = false;
		const auto buzzerPin = hardware::GetLED(LED::BUZZER);
		for(auto& duration : durations)
		{
			const auto sleepDuration = std::chrono::milliseconds(duration);
			if(!isOn)
			{
//				SetDigitalOut(buzzerPin, true);
				std::this_thread::sleep_for(sleepDuration);
				isOn = true;
			}
			else
			{
//				SetDigitalOut(buzzerPin, false);
				std::this_thread::sleep_for(sleepDuration);
				isOn = false;
			}
		}
//		SetDigitalOut(buzzerPin, false);
	}
}

} // namespace mandeye