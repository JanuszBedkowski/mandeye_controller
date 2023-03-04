#include "web_server.h"
#include <chrono>
#include <json.hpp>
#include <ostream>
#include <thread>

#include <LivoxClient.h>
#include <gpios.h>
#include <iostream>
namespace mandeye {

    std::shared_ptr<LivoxClient> livoxCLientPtr;
    std::shared_ptr<GpioClient> gpioClientPtr;


    using json = nlohmann::json;

    std::string produceReport() {
        json j;
        j["name"] = "Mandye";
        if (livoxCLientPtr) {
            j["livox"] = livoxCLientPtr->produceStatus();
        } else {
            j["livox"] = {};
        }

        if (gpioClientPtr) {
            j["gpio"] = gpioClientPtr->produceStatus();
        }
        std::ostringstream s;
        s << std::setw(4) << j;
        return s.str();
    }

} // namespace mandeye

namespace utils{
    bool getEnvBool(const std::string env){
        const char* env_p = std::getenv(env.c_str());
        if (env_p == nullptr){
            return false;
        }
        if (strcmp("1",env_p)==0){
            return true;
        }
    }
}

int main(int argc, char **argv) {

    health_server::setStatusHandler(mandeye::produceReport);
    std::thread http_thread1(health_server::server_worker);

    std::thread thLivox([&]() {
        mandeye::livoxCLientPtr = std::make_shared<mandeye::LivoxClient>();
        mandeye::livoxCLientPtr->startListener();
    });

    std::thread thGpio([&]() {
        const bool simMode = utils::getEnvBool("MANDEYE_GPIO_SIM");
        std::cout << "MANDEYE_GPIO_SIM : " << simMode << std::endl;
        mandeye::gpioClientPtr = std::make_shared<mandeye::GpioClient>(simMode);
        std::cout << "GPIO Init done" << std::endl;
    });
    bool done = false;

    std::thread test([&]() {
        while(!done) {
            if (mandeye::gpioClientPtr) {
                using namespace std::literals::chrono_literals;
                mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, true);
                std::this_thread::sleep_for(100ms);
                mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_RED, true);
                std::this_thread::sleep_for(100ms);
                mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, true);
                std::this_thread::sleep_for(1000ms);
                mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, false);
                mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_RED, false);
                mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, false);
                std::this_thread::sleep_for(50ms);
            }
        }
    });

    std::cout << "Press eny key to end" << std::endl;

    std::getchar();
    done =true;
    std::cout << "joining thLivox" << std::endl;
    thLivox.join();

    std::cout << "joining thGpio" << std::endl;
    thGpio.join();
    test.join();
    health_server::done = true;
    std::cout << "Done" << std::endl;
    return 0;
}