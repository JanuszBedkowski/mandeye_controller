#include <iostream>
#include <gpios.h>
#include <thread>
#include <chrono>

std::mutex gpioClientPtrLock;
std::shared_ptr<mandeye::GpioClient> gpioClientPtr;
bool StopScan()
{
	gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_STOP_SCAN, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
    gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_STOP_SCAN, false);
	return false;
}

bool Continous()
{
	gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_CONTINOUS_SCANNING, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
    gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_CONTINOUS_SCANNING, false);
	
    return false;
}


int main(int arc, char *argv[]){
    std::cout << "button_demo" << std::endl;

    gpioClientPtr = std::make_shared<mandeye::GpioClient>(0);
    gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_STOP_SCAN, false);
    gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_COPY_DATA, false);
    gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_CONTINOUS_SCANNING, false);

    //gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_BLUE, true);
    gpioClientPtr->addButtonCallback(mandeye::GpioClient::BUTTON::BUTTON_STOP_SCAN, "LED_GPIO_STOP_SCAN", [&]() { StopScan(); });
    gpioClientPtr->addButtonCallback(mandeye::GpioClient::BUTTON::BUTTON_CONTINOUS_SCANNING, "STOP_SCAN", [&]() { Continous(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100000)); 

    return 0;
}
