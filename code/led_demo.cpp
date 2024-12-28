#include <iostream>
#include <gpios.h>
#include <thread>
#include <chrono>


int main(int arc, char *argv[]){
	using namespace hardware;
    std::cout << "led_demo" << std::endl;
    std::cout << mandeyeHarwareType << std::endl;

    std::shared_ptr<mandeye::GpioClient> gpioClientPtr;

    gpioClientPtr = std::make_shared<mandeye::GpioClient>(false);
    for(int i = 0; i < 1000; i++)
    {
        std::cout << "iteration " << i + 1  << " of 10" << std::endl;
        std::cout << "LED_GPIO_STOP_SCAN ON" << std::endl;
        gpioClientPtr->setLed(LED::LED_GPIO_STOP_SCAN, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		std::cout << "LED_GPIO_COPY_DATA ON" << std::endl;
        gpioClientPtr->setLed(LED::LED_GPIO_COPY_DATA, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "LED_GPIO_CONTINOUS_SCANNING ON" << std::endl;
        gpioClientPtr->setLed(LED::LED_GPIO_CONTINOUS_SCANNING, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

		std::cout << "BUZZER ON" << std::endl;
		gpioClientPtr->setLed(LED::BUZZER, true);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));

		std::cout << "BUZZER OFF" << std::endl;
		gpioClientPtr->setLed(LED::BUZZER, false);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));


		std::cout << "LED_GPIO_STOP_SCAN OFF" << std::endl;
        gpioClientPtr->setLed(LED::LED_GPIO_STOP_SCAN, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "LED_GPIO_COPY_DATA OFF" << std::endl;
        gpioClientPtr->setLed(LED::LED_GPIO_COPY_DATA, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "LED_GPIO_CONTINOUS_SCANNING OFF" << std::endl;
        gpioClientPtr->setLed(LED::LED_GPIO_CONTINOUS_SCANNING, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));


    }
    
    return 0;
}
