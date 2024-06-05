#include <iostream>
#include <gpios.h>
#include <thread>
#include <chrono>

std::mutex gpioClientPtrLock;
std::shared_ptr<mandeye::GpioClient> gpioClientPtr;
using namespace hardware;
bool StopScan()
{
	gpioClientPtr->setLed(LED::LED_GPIO_STOP_SCAN, true);
	gpioClientPtr->setLed(LED::BUZZER, true);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
    gpioClientPtr->setLed(LED::LED_GPIO_STOP_SCAN, false);
	gpioClientPtr->setLed(LED::BUZZER, false);
	return false;
}

bool Continous()
{
	gpioClientPtr->setLed(LED::LED_GPIO_CONTINOUS_SCANNING, true);
	gpioClientPtr->setLed(LED::BUZZER, true);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
    gpioClientPtr->setLed(LED::LED_GPIO_CONTINOUS_SCANNING, false);
	gpioClientPtr->setLed(LED::BUZZER, false);
	return false;
}


int main(int arc, char *argv[]){
    std::cout << "button_demo" << std::endl;

    gpioClientPtr = std::make_shared<mandeye::GpioClient>(0);
    gpioClientPtr->setLed(LED::LED_GPIO_STOP_SCAN, false);
    gpioClientPtr->setLed(LED::LED_GPIO_COPY_DATA, false);
    gpioClientPtr->setLed(LED::LED_GPIO_CONTINOUS_SCANNING, false);

    //gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_BLUE, true);
    gpioClientPtr->addButtonCallback(BUTTON::BUTTON_STOP_SCAN, "LED_GPIO_STOP_SCAN", [&]() { StopScan(); });
    gpioClientPtr->addButtonCallback(BUTTON::BUTTON_CONTINOUS_SCANNING, "STOP_SCAN", [&]() { Continous(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100000)); 

    return 0;
}
