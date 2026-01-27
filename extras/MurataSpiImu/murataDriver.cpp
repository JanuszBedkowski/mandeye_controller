#include "SCH1.h"
#include "hw.h"
#include <json.hpp>
#include <pistache/endpoint.h>
#include <zmq.hpp>
#include <json.hpp>
#include <fstream>
#include <functional>
namespace  fs = std::filesystem;

namespace {
    std::string getEnvString(const std::string &env, const std::string &def) {
        const char *env_p = std::getenv(env.c_str());
        if (env_p == nullptr) {
            return def;
        }
        return std::string{env_p};
    }
} // namespace
namespace global {
    std::string directoryName = "MURATA_IMU";
    int fileLengthMs = 20000; // 20 seconds
}

namespace MODES {
    const static char* SCANNING = "SCANNING";
    const static char* STOPPING = "STOPPING";
    const static char* UNKNOWN = "UNKNOWN";

    const auto SCANNING_ID = std::hash<std::string>{}(SCANNING);
    const auto STOPPING_ID = std::hash<std::string>{}(STOPPING);
    const auto UNKNOWN_ID = std::hash<std::string>{}(UNKNOWN);

}

namespace state {
    std::mutex stateMutex;
    std::string modeName = MODES::UNKNOWN;
    auto hashCode = MODES::UNKNOWN_ID;
    double timestamp;
    std::string continuousScanTarget;
} // namespace state

nlohmann::json getConfig(const std::string& configPath)
{
    if (!std::filesystem::exists(configPath))
    {
        std::cerr<< "Config file does not exist" << std::endl;
        return nlohmann::json();
    }
    nlohmann::json configJson;
    std::ifstream configFile(configPath);
    if (configFile.is_open())
    {
        try
        {
            configFile >> configJson;
        }
        catch (nlohmann::json::parse_error& e)
        {
            std::cerr << "Error parsing config file: " << e.what() << std::endl;
            return nlohmann::json();
        }
        configFile.close();
        return configJson;
    }
    else
    {
        std::cerr << "Failed to open config file at " << configPath << std::endl;
        return nlohmann::json();
    }
    return nlohmann::json();
}

void clientThread() {
    try {
        zmq::context_t context(1);
        zmq::socket_t socket(context, zmq::socket_type::sub);

        // Connect to the publisher
        socket.connect("tcp://localhost:5556");

        // Subscribe to all messages (empty filter)
        socket.set(zmq::sockopt::subscribe, "");

        // Set CONFLATE option (keep only last message)
        socket.set(zmq::sockopt::conflate, 1);

        std::cout << "Connected to tcp://localhost:5556" << std::endl;
        std::cout << "Waiting for messages..." << std::endl;

        while (true) {
            // Wait for a message
            zmq::message_t message;
            auto result = socket.recv(message, zmq::recv_flags::none);

            if (result) {
                std::string msg_str(static_cast<char *>(message.data()), message.size());

                nlohmann::json j = nlohmann::json::parse(msg_str);
                if (j.is_object()) {
                    std::lock_guard<std::mutex> lck(state::stateMutex);
                    if (j.contains("time")) {
                        state::timestamp = j["time"].get<double>();
                    }
                    if (j.contains("mode")) {
                        state::modeName = j["mode"].get<std::string>();
                        // hash mode
                        state::hashCode = std::hash<std::string>{}(state::modeName);
                    }
                    if (j.contains("continousScanDirectory")) {
                        state::continuousScanTarget = j["continousScanDirectory"].get<std::string>();
                    }
                }
            }
        }
    } catch (const zmq::error_t &e) {
        std::cerr << "ZeroMQ Error: " << e.what() << std::endl;
        std::abort();
    }
}

int murataThread() {
    hw_init();

    char serial_num[15];
    int init_status;
    SCH1_filter Filter;
    SCH1_sensitivity Sensitivity;
    SCH1_decimation Decimation;

    // SCH1600 settings and initialization
    //------------------------------------

    // SCH1600 filter settings
    Filter.Rate12 = FILTER_RATE;
    Filter.Acc12 = FILTER_ACC12;
    Filter.Acc3 = FILTER_ACC3;

    // SCH1600 sensitivity settings
    Sensitivity.Rate1 = SENSITIVITY_RATE1;
    Sensitivity.Rate2 = SENSITIVITY_RATE2;
    Sensitivity.Acc1 = SENSITIVITY_ACC1;
    Sensitivity.Acc2 = SENSITIVITY_ACC2;
    Sensitivity.Acc3 = SENSITIVITY_ACC3;

    // SCH1600 decimation settings (for Rate2 and Acc2 channels).
    Decimation.Rate2 = DECIMATION_RATE;
    Decimation.Acc2 = DECIMATION_ACC;

    // Initialize the sensor
    init_status = SCH1_init(Filter, Sensitivity, Decimation, false);
    if (init_status != SCH1_OK) {
        printf("ERROR: SCH1_init failed with code: %d\r\nApplication halted\r\n", init_status);
        return -1;
    }

    strcpy(serial_num, SCH1_getSnbr());
    printf("Serial number: %s\r\n\r\n", serial_num);
    auto start = std::chrono::high_resolution_clock::now();
    int count = 0;
    auto last = start;
    auto fileStartTimeStamp = std::chrono::high_resolution_clock::now();
    std::ofstream logFile;
    std::string logFileName;
    for (;;) {
        SCH1_raw_data SCH1_data;
        SCH1_getData(&SCH1_data);
        SCH1_result result;
        SCH1_convert_data(&SCH1_data, &result);
        if (SCH1_data.frame_error == false) {
            // std::cout << result.Acc1[AXIS_X] << " " << result.Acc1[AXIS_Y] << " " << result.Acc1[AXIS_Z] << std::endl;
            count++;
        } else {
            std::cout << "Frame error" << std::endl;
        }
        auto now = std::chrono::high_resolution_clock::now();
        double dt = std::chrono::duration<double>(now - last).count();

        static auto lastTime = std::chrono::steady_clock::now();

        // get current mode
        std::string mode;
        std::string continousScanTarget;
        {
            std::lock_guard lck(state::stateMutex);
            continousScanTarget = state::continuousScanTarget;
            mode = state::modeName;
        }

        {
            static auto old_state_id = MODES::UNKNOWN_ID;
            auto state_id = MODES::UNKNOWN_ID;
            {
                std::lock_guard lock(state::stateMutex);
                state_id = state::hashCode;
            }

            if (mode == MODES::SCANNING && !logFile.is_open()) {
                std::lock_guard lock(state::stateMutex);
                std::cout << "Mode: " << mode << std::endl;
                // construct path
                const fs::path directory = fs::path(continousScanTarget)/ global::directoryName;
                // mkdir -p
                std::filesystem::create_directories(directory);
                fileStartTimeStamp = std::chrono::high_resolution_clock::now();
                logFileName = directory.string() + "/" + "murata_imu_" + std::to_string(start.time_since_epoch().count()) + ".csv";
                std::cout << "Logging to " << logFileName << std::endl;
                logFile.open(logFileName);
                logFile << "timestamp gyroX gyroY gyroZ accX accY accZ" << std::endl;
            }
            if (logFile.is_open()) {
                const uint64_t timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
                logFile << timestamp << " ";
                logFile << result.Rate1[AXIS_X] << " ";
                logFile << result.Rate1[AXIS_Y] << " ";
                logFile << result.Rate1[AXIS_Z] << " ";
                logFile << result.Acc1[AXIS_X] << " ";
                logFile << result.Acc1[AXIS_Y] << " ";
                logFile << result.Acc1[AXIS_Z] << std::endl;

                const auto now = std::chrono::high_resolution_clock::now();
                const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - fileStartTimeStamp);
                if (duration.count() > global::fileLengthMs) {
                    std::cout << "Closing file " <<logFileName << std::endl;
                    logFile.close();
                }
            }
            if (mode == MODES::STOPPING && logFile.is_open()) {
                std::cout << "Closing file " << logFileName << std::endl;
                logFile.close();
            }
            old_state_id = state_id;

            // FusionVector g;
            // g.axis.x = result.Rate1[AXIS_X];
            // g.axis.y = result.Rate1[AXIS_Y];
            // g.axis.z = result.Rate1[AXIS_Z];
            // gyro = g;
            // FusionVector a;
            // a.axis.x = result.Acc1[AXIS_X] ;
            // a.axis.y = result.Acc1[AXIS_Y] ;
            // a.axis.z = result.Acc1[AXIS_Z] ;
            // acc = a;
            // gyro_acc[0] += g.axis.x;
            // gyro_acc[1] += g.axis.y;
            // gyro_acc[2] += g.axis.z;
            // number_of_samples ++;
            //
            // FusionVector gyro_bias_corrected;
            // gyro_bias_corrected.axis.x = gyro.axis.x - gyro_bias.axis.x;
            // gyro_bias_corrected.axis.y = gyro.axis.y - gyro_bias.axis.y;
            // gyro_bias_corrected.axis.z = gyro.axis.z - gyro_bias.axis.z;
            // FusionAhrsUpdateNoMagnetometer(&ahrs, gyro_bias_corrected, a, dt);
        }
        auto sleep_deadline = last + std::chrono::milliseconds(1);
        std::this_thread::sleep_until(sleep_deadline);

        last = now;

        std::chrono::duration<double> elapsed_seconds = now - start;
        if (elapsed_seconds > std::chrono::seconds(1)) {
            std::cout << "FPS: " << count << std::endl;
            count = 0;
            start = std::chrono::high_resolution_clock::now();
        }
    }
}

int main(int argc, char **argv) {

    std::string configPath = getEnvString("EXTRA_GNSS_CONFIG_PATH", "/media/usb/config_murata_imu.json");
    std::cout << "Loading configuration from usb : " << configPath << std::endl;
    global::directoryName = getEnvString("EXTRA_GNSS_DIRECTORY_NAME", "MURATA_IMU");

    nlohmann::json configJson;
    bool configOk = false;
    try
    {
        configJson = getConfig(configPath);
        if (configJson.is_object() && !configJson.empty())
        {
            std::cout << "Loaded configuration" << std::endl;
            std::cout << configJson.dump(4) << std::endl;
            global::fileLengthMs = configJson["file_length_ms"].get<int>();
            configOk = true;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to load config: " << e.what() << std::endl;
    }
    if (!configOk)
    {
        // create a valid config
        configJson["file_length_ms"] = global::fileLengthMs;
        std::ofstream configFile(configPath);
        configFile << configJson.dump(4);
        std::cout << "Created default config at " << configPath << std::endl;
    }
    std::cout << "Starting GNSS client" << std::endl;

    std::thread tzmq(clientThread);
    std::thread timu(murataThread);
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    timu.join();
    tzmq.join();
}
