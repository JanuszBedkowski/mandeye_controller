#include <iostream>
#include <gpios.h>
#include <thread>
#include <chrono>


int main(int arc, char *argv[]){
    std::cout << "led_demo" << std::endl;

    std::shared_ptr<mandeye::GpioClient> gpioClientPtr;

    gpioClientPtr = std::make_shared<mandeye::GpioClient>(false);
    for(int i = 0; i < 10; i++)
    {
        std::cout << "iteration " << i + 1  << " of 10" << std::endl;
        std::cout << "red ON" << std::endl;
        gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_RED, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "green ON" << std::endl;
        gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "blue ON" << std::endl;
        gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_BLUE, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "yellow ON" << std::endl;
        gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        std::cout << "red OFF" << std::endl;
        gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_RED, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "green OFF" << std::endl;
        gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "blue OFF" << std::endl;
        gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_BLUE, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "yellow OFF" << std::endl;
        gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        

        /*std::this_thread::sleep_for(100ms);
        mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_RED, true);
        std::this_thread::sleep_for(100ms);
        mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, true);
        std::this_thread::sleep_for(1000ms);
        mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, false);
        std::this_thread::sleep_for(100ms);
        mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_RED, false);
        std::this_thread::sleep_for(100ms);
        mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, false);*/
    }
    std::cout << "GPIO Init done" << std::endl;

    return 0;
}