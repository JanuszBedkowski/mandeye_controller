#include <iostream>
#include <gpios.h>
#include <thread>
#include <chrono>

bool StartScan()
{
	std::cout << "BUTTON 1" << std::endl;
    exit(1);
	return false;
}

bool StopScan()
{
	std::cout << "BUTTON 2" << std::endl;
    exit(2);
	return false;
}

std::mutex gpioClientPtrLock;
std::shared_ptr<mandeye::GpioClient> gpioClientPtr;

int main(int arc, char *argv[]){
    std::cout << "button_demo" << std::endl;

    std::thread thGpio([&]() {
		std::lock_guard<std::mutex> l2(gpioClientPtrLock);
		using namespace std::chrono_literals;
		
		gpioClientPtr = std::make_shared<mandeye::GpioClient>(1);
		gpioClientPtr->addButtonCallback(mandeye::GpioClient::BUTTON::BUTTON_1, "START_SCAN", [&]() { StartScan(); });
		gpioClientPtr->addButtonCallback(mandeye::GpioClient::BUTTON::BUTTON_2, "STOP_SCAN", [&]() { StopScan(); });
	});

    while(1){
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    thGpio.join();

    return 0;
}