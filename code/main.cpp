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
    enum class States {
        WAIT_FOR_RESOURCES = -10,
        IDLE = 0,
        STARTING_SCAN = 10,
        SCANNING = 20,
        STOPPING = 30,
        STOPPED = 40,
    };

    const std::map<States, std::string> StatesToString{
            {States::WAIT_FOR_RESOURCES, "WAIT_FOR_RESOURCES"},
            {States::IDLE, "IDLE"},
            {States::STARTING_SCAN, "STARTING_SCAN"},
            {States::SCANNING, "SCANNING"},
            {States::STOPPING, "STOPPING"},
            {States::STOPPED, "STOPPED"},
    };

    std::atomic<bool> isRunning{true};
    std::mutex livoxClientPtrLock;
    std::shared_ptr<LivoxClient> livoxCLientPtr;
    std::mutex gpioClientPtrLock;
    std::shared_ptr<GpioClient> gpioClientPtr;
    mandeye::States app_state{mandeye::States::WAIT_FOR_RESOURCES};

    using json = nlohmann::json;

    std::string produceReport() {
        json j;
        j["name"] = "Mandye";
        j["state"] = StatesToString.at(app_state);
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


    bool StartScan (){
        if (app_state == States::IDLE || app_state ==  States::STOPPED)
        {
            app_state = States::STARTING_SCAN;
            return true;
        }
        return false;
    }
    bool StopScan (){
        if (app_state == States::SCANNING)
        {
            app_state = States::STOPPING;
            return true;
        }
        return false;
    }

    void savePointcloudData(LivoxPointsBufferPtr buffer){
        using namespace std::chrono_literals;
        std::cout << "Savig buffer of size " << buffer->size() << std::endl;
        std::this_thread::sleep_for(2s);
    }

    void stateWatcher(){
        using namespace std::chrono_literals;
        auto chunkStart = std::chrono::steady_clock::now();
        States oldState = States::IDLE;
        while (isRunning){
            if (oldState!=app_state){
                std::cout <<"State transtion from " << StatesToString.at(oldState) <<" to "<< StatesToString.at(app_state) << std::endl;
            }
            oldState = app_state;
            if(app_state == States::WAIT_FOR_RESOURCES){
                std::this_thread::sleep_for(100ms);
                std::lock_guard<std::mutex> l1(livoxClientPtrLock);
                std::lock_guard<std::mutex> l2(gpioClientPtrLock);
                if (mandeye::gpioClientPtr && mandeye::gpioClientPtr){
                    app_state = States::IDLE;
                }
            }else if(app_state == States::IDLE){

                mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, false);
                mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_RED, true);
                mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, false);

                std::this_thread::sleep_for(100ms);
            }
            else if(app_state == States::STARTING_SCAN)
            {

                mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, false);
                mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_RED, false);
                mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, false);

                if(livoxCLientPtr){
                    livoxCLientPtr->startLog();
                    app_state = States::SCANNING;
                }
                chunkStart = std::chrono::steady_clock::now();
            }
            else if(app_state == States::SCANNING){
                if (gpioClientPtr) {
                    mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, true);
                }
                const auto now = std::chrono::steady_clock::now();
                if (now - chunkStart > std::chrono::seconds(15) ){
                    mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, true);
                    chunkStart = std::chrono::steady_clock::now();
                    auto lidarBuffer = livoxCLientPtr->retrieveCollectedLidarData();
                    savePointcloudData(lidarBuffer);
                    mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, false);
                }
                std::this_thread::sleep_for(100ms);
            }
            else if(app_state == States::STOPPING){
                mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, true);
                auto lidarBuffer = livoxCLientPtr->retrieveCollectedLidarData();
                livoxCLientPtr->stopLog();
                savePointcloudData(lidarBuffer);
                mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, false);
                app_state = States::IDLE;
            }

        }
    }


} // namespace mandeye

namespace utils {
    std::string getEnvString(const std::string &env, const std::string &def) {
        const char *env_p = std::getenv(env.c_str());
        if (env_p == nullptr) {
            return def;
        }
        return std::string{env_p};
    }

    bool getEnvBool(const std::string &env, bool def) {
        const char *env_p = std::getenv(env.c_str());
        if (env_p == nullptr) {
            return def;
        }
        if (strcmp("1", env_p) == 0 || strcmp("true", env_p) == 0) {
            return true;
        }
        return false;
    }
}


int main(int argc, char **argv) {



    health_server::setStatusHandler(mandeye::produceReport);
    std::thread http_thread1(health_server::server_worker);

    std::thread thLivox([&]() {
        {
            std::lock_guard<std::mutex> l1(mandeye::livoxClientPtrLock);
            mandeye::livoxCLientPtr = std::make_shared<mandeye::LivoxClient>();
        }
        mandeye::livoxCLientPtr->startListener(utils::getEnvString("MANDEYE_LIVOX_LISTEN_IP", "192.168.1.50"));
    });

    std::thread thStateMachine([&]() {
        mandeye::stateWatcher();
    });

    std::thread thGpio([&]() {
        std::lock_guard<std::mutex> l2(mandeye::gpioClientPtrLock);
        using namespace std::chrono_literals;
        const bool simMode = utils::getEnvBool("MANDEYE_GPIO_SIM", false);
        std::cout << "MANDEYE_GPIO_SIM : " << simMode << std::endl;
        mandeye::gpioClientPtr = std::make_shared<mandeye::GpioClient>(simMode);
        for (int i = 0; i < 3; i++) {
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

        mandeye::gpioClientPtr->addButtonCallback(mandeye::GpioClient::BUTTON::BUTTON_1, "START_SCAN", [&]() {
            mandeye::StartScan();
        });
        mandeye::gpioClientPtr->addButtonCallback(mandeye::GpioClient::BUTTON::BUTTON_2, "STOP_SCAN", [&]() {
            mandeye::StopScan();
        });

    });


    while(mandeye::isRunning){
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1000ms);
        std::cout << "Press q -> quit, s -> start scan , e -> end scan" << std::endl;
        char ch = std::getchar();
        if (ch == 'q'){
            mandeye::isRunning.store(false);
        }else if (ch == 's'){
            if (mandeye::StartScan()){
                std::cout <<"start scan success!" << std::endl;
            }
        }else if (ch == 'e'){
            if (mandeye::StopScan()){
                std::cout <<"stop scan success!" << std::endl;
            }
        }
    }

    std::cout << "joining thStateMachine" << std::endl;
    thStateMachine.join();

    std::cout << "joining thLivox" << std::endl;
    thLivox.join();

    std::cout << "joining thGpio" << std::endl;
    thGpio.join();
    health_server::done = true;
    std::cout << "Done" << std::endl;
    return 0;
}