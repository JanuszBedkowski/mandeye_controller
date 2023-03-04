#pragma once
#include <json.hpp>
#include <mutex>
#include <cppgpio/output.hpp>
#include <cppgpio/buttons.hpp>
#include <unordered_map>
namespace mandeye {

    // forward declaration of cppgpio type that I want to keep inside compliation unit

    class GpioClient {
    public:
        enum class LED{
            LED_GPIO_RED,
            LED_GPIO_GREEN,
            LED_GPIO_YELLOW
        };

        enum class BUTTON{
            BUTTON_1,
            BUTTON_2
        };

    public:
        //! Constructor
        //! @param sim if true hardware is not called
        GpioClient(bool sim);

        //! serialize component state to API
        nlohmann::json produceStatus();

        //! set led to given state
        void setLed(LED led, bool state);

        //! addcalback
        void addButtonCallback(BUTTON btn, const std::string& callbackName, const std::function<void()>& callback);


    private:
        using Callbacks=std::unordered_map<std::string,std::function<void()>>;

        //! use simulated GPIOs instead real one
        bool m_useSimulatedGPIO{true};

        std::unordered_map<LED, bool> m_ledState{
                {LED::LED_GPIO_RED, false},
                {LED::LED_GPIO_GREEN, false},
                {LED::LED_GPIO_YELLOW, false},
        };

        //! available LEDs
        std::unordered_map<GpioClient::LED, std::unique_ptr<GPIO::DigitalOut>> m_ledGpio;

        //! available Buttons
        std::unordered_map<GpioClient::BUTTON, std::unique_ptr<GPIO::PushButton>> m_buttons;

        std::unordered_map<GpioClient::BUTTON, Callbacks> m_buttonsCallbacks;



        //! useful translations
        const std::unordered_map<LED, std::string> LedToName{
                {LED::LED_GPIO_RED,"LED_GPIO_RED"},
                {LED::LED_GPIO_GREEN,"LED_GPIO_GREEN"},
                {LED::LED_GPIO_YELLOW,"LED_GPIO_YELLOW"},
        };

        const std::unordered_map<std::string,LED> NameToLed{
                {"LED_GPIO_RED",LED::LED_GPIO_RED},
                {"LED_GPIO_GREEN",LED::LED_GPIO_GREEN},
                {"LED_GPIO_YELLOW",LED::LED_GPIO_YELLOW},
        };

        const std::unordered_map<BUTTON, std::string> ButtonToName{
                {BUTTON::BUTTON_1,"BUTTON_1"},
                {BUTTON::BUTTON_2,"BUTTON_2"},
        };

        const std::unordered_map<std::string,BUTTON> NameToButton{
                {"BUTTON_1",BUTTON::BUTTON_1},
                {"BUTTON_2",BUTTON::BUTTON_2},
        };


        std::mutex m_lock;
    };
} // namespace mandeye