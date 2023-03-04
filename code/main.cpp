#include "web_server.h"
#include <chrono>
#include <json.hpp>
#include <ostream>
#include <thread>

#include <LivoxClient.h>
#include <gpios.h>
#include <iostream>
#include <string>

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
    std::string getEnvString(const std::string& env, const std::string& def){
        const char* env_p = std::getenv(env.c_str());
        if (env_p == nullptr){
            return def;
        }
        return std::string {env_p};
    }

    bool getEnvBool(const std::string& env, bool def){
        const char* env_p = std::getenv(env.c_str());
        if (env_p == nullptr){
            return def;
        }
        if (strcmp("1",env_p)==0 || strcmp("true",env_p)==0 ){
            return true;
        }
        return false;
    }
}

void makeStopScan(){
    if(mandeye::gpioClientPtr){
        mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, true);
    }
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(500ms);

}

int main(int argc, char **argv) {

    health_server::setStatusHandler(mandeye::produceReport);
    std::thread http_thread1(health_server::server_worker);

    std::thread thLivox([&]() {
        mandeye::livoxCLientPtr = std::make_shared<mandeye::LivoxClient>();
        mandeye::livoxCLientPtr->startListener(utils::getEnvString("MANDEYE_LIVOX_LISTEN_IP", "192.168.1.50"));
    });

    std::thread thGpio([&]() {
        using namespace std::chrono_literals;
        const bool simMode = utils::getEnvBool("MANDEYE_GPIO_SIM", false);
        std::cout << "MANDEYE_GPIO_SIM : " << simMode << std::endl;
        mandeye::gpioClientPtr = std::make_shared<mandeye::GpioClient>(simMode);
        for (int i =0; i < 5; i++) {
            mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, true);
            std::this_thread::sleep_for(100ms);
            mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_RED, true);
            std::this_thread::sleep_for(100ms);
            mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, true);
            std::this_thread::sleep_for(1000ms);
            mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, false);
            std::this_thread::sleep_for(100ms);
            mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_RED, false);
            std::this_thread::sleep_for(100ms);
            mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, false);
        }
        std::cout << "GPIO Init done" << std::endl;

        mandeye::gpioClientPtr->addButtonCallback(mandeye::GpioClient::BUTTON::BUTTON_1, "START_SCAN", [&](){
            mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, true);
            std::this_thread::sleep_for(100ms);
            mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, false);
        });
    });


    std::cout << "Press eny key to end" << std::endl;

    std::getchar();
    std::cout << "joining thLivox" << std::endl;
    thLivox.join();

    std::cout << "joining thGpio" << std::endl;
    thGpio.join();
    health_server::done = true;
    std::cout << "Done" << std::endl;
    return 0;
}