#include <fstream>
#include <gpios.h>
#include <iostream>

void GPIO::CreateDigitalIn(int pin)
{
	std::string command = "raspi-gpio set " + std::to_string(pin) + " ip";
	int ret = std::system(command.c_str());
	std::cout << "Called " << command << " return " << ret << std::endl;
	pin += Offset;
	if(pin > 0)
	{
		// export the pin
		std::ofstream exportFile("/sys/class/gpio/export");
		exportFile << pin;
		exportFile.close();

		// set the direction
		std::ofstream directionFile("/sys/class/gpio/gpio" + std::to_string(pin) + "/direction");
		directionFile << "in";
		directionFile.close();
	}
}

void GPIO::CreateDigitalOut(int pin)
{
	std::string command = "raspi-gpio set " + std::to_string(pin) + " op";
	int ret = std::system(command.c_str());
	std::cout << "Called " << command << " return " << ret << std::endl;
	pin += Offset;
	if(pin > 0)
	{
		// export the pin
		std::ofstream exportFile("/sys/class/gpio/export");
		exportFile << pin;
		exportFile.close();

		// set the direction
		std::ofstream directionFile("/sys/class/gpio/gpio" + std::to_string(pin) + "/direction");
		directionFile << "out";
		directionFile.close();
	}
}

void GPIO::SetDigitalOut(int pin, bool value)
{
	pin += Offset;
	if(pin > 0)
	{
		std::ofstream valueFile("/sys/class/gpio/gpio" + std::to_string(pin) + "/value");
		valueFile << value;
		valueFile.close();
	}
}

bool GPIO::GetDigitalIn(int pin, int& value)
{
	pin += Offset;
	if(pin > 0)
	{
		std::ifstream valueFile("/sys/class/gpio/gpio" + std::to_string(pin) + "/value");
		valueFile >> value;
		valueFile.close();
	}
	return true;
}

void GPIO::setPullUp(int gpio, GPIO::GPIO_PULL pull)
{

	std::string command = "raspi-gpio set " + std::to_string(gpio);
	if(pull == GPIO::GPIO_PULL::OFF)
	{
		command += " pn";
	}
	else if(pull == GPIO::GPIO_PULL::DOWN)
	{
		command += " pd";
	}
	else if(pull == GPIO::GPIO_PULL::UP)
	{
		command += " pu";
	}
	int ret = std::system(command.c_str());
	std::cout << "Called " << command << " return " << ret << std::endl;
}

namespace mandeye
{
using namespace hardware;
using namespace GPIO;
bool GpioClient::ButtonData::GetButtonState(const GpioClient::ButtonData& button)
{
	int rawButtonState = -1;
	GetDigitalIn(button.m_pin, rawButtonState);
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
		std::lock_guard<std::mutex> lck{m_lock};

		CreateDigitalOut(hardware::GetLED(LED::LED_GPIO_STOP_SCAN));
		CreateDigitalOut(hardware::GetLED(LED::LED_GPIO_COPY_DATA));
		CreateDigitalOut(hardware::GetLED(LED::LED_GPIO_CONTINOUS_SCANNING));
		//
		CreateDigitalOut(hardware::GetLED(LED::BUZZER));
		CreateDigitalOut(hardware::GetLED(LED::LIDAR_SYNC_1));
		CreateDigitalOut(hardware::GetLED(LED::LIDAR_SYNC_2));

		// set pull

		CreateDigitalIn(hardware::GetButton(BUTTON::BUTTON_STOP_SCAN));
		CreateDigitalIn(hardware::GetButton(BUTTON::BUTTON_CONTINOUS_SCANNING));

		m_buttons[BUTTON::BUTTON_STOP_SCAN].m_pin = hardware::GetButton(BUTTON::BUTTON_STOP_SCAN);
		m_buttons[BUTTON::BUTTON_CONTINOUS_SCANNING].m_pin = hardware::GetButton(BUTTON::BUTTON_CONTINOUS_SCANNING);

		m_buttons[BUTTON::BUTTON_STOP_SCAN].m_pullMode = hardware::GetPULL(BUTTON::BUTTON_STOP_SCAN);
		m_buttons[BUTTON::BUTTON_CONTINOUS_SCANNING].m_pullMode = hardware::GetPULL(BUTTON::BUTTON_CONTINOUS_SCANNING);

		// set pull
		for(auto& [_, button] : m_buttons)
		{
			setPullUp(button.m_pin, button.m_pullMode);
		}

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
		SetDigitalOut(hardware::GetLED(led), state);
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
				SetDigitalOut(buzzerPin, true);
				std::this_thread::sleep_for(sleepDuration);
				isOn = true;
			}
			else
			{
				SetDigitalOut(buzzerPin, false);
				std::this_thread::sleep_for(sleepDuration);
				isOn = false;
			}
		}
		SetDigitalOut(buzzerPin, false);
	}
}

} // namespace mandeye