#include <iostream>
#include <gpios.h>
#include <thread>
#include <chrono>

std::mutex gpioClientPtrLock;
std::shared_ptr<mandeye::GpioClient> gpioClientPtr;
bool StartScan()
{
	gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_RED, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
    gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_RED, false);
	return false;
}

bool StopScan()
{
	gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
    gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, false);
	
    return false;
}


int main(int arc, char *argv[]){
    std::cout << "button_demo" << std::endl;


    gpioClientPtr = std::make_shared<mandeye::GpioClient>(0);
    gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_BLUE, true);
    gpioClientPtr->addButtonCallback(mandeye::GpioClient::BUTTON::BUTTON_1, "START_SCAN", [&]() { StartScan(); });
    gpioClientPtr->addButtonCallback(mandeye::GpioClient::BUTTON::BUTTON_2, "STOP_SCAN", [&]() { StopScan(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100000)); 

    return 0;
}
