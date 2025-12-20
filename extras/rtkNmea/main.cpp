#include <zmq.hpp>
#include <pistache/endpoint.h>
#include <zmq.hpp>
#include <json.hpp>
#include "gnss.h"
#include <pistache/endpoint.h>
#include "fstream"


static const std::unordered_map<int, LibSerial::BaudRate> baud_map = {
    {50,   LibSerial::BaudRate::BAUD_50},
    {75,   LibSerial::BaudRate::BAUD_75},
    {110,  LibSerial::BaudRate::BAUD_110},
    {134,  LibSerial::BaudRate::BAUD_134},
    {150,  LibSerial::BaudRate::BAUD_150},
    {200,  LibSerial::BaudRate::BAUD_200},
    {300,  LibSerial::BaudRate::BAUD_300},
    {600,  LibSerial::BaudRate::BAUD_600},
    {1200, LibSerial::BaudRate::BAUD_1200},
    {1800, LibSerial::BaudRate::BAUD_1800},
    {2400, LibSerial::BaudRate::BAUD_2400},
    {4800, LibSerial::BaudRate::BAUD_4800},
    {9600, LibSerial::BaudRate::BAUD_9600},
    {19200,  LibSerial::BaudRate::BAUD_19200},
    {38400,  LibSerial::BaudRate::BAUD_38400},
    {57600,  LibSerial::BaudRate::BAUD_57600},
    {115200, LibSerial::BaudRate::BAUD_115200},
    {230400, LibSerial::BaudRate::BAUD_230400},
    {460800,  LibSerial::BaudRate::BAUD_460800},
    {500000,  LibSerial::BaudRate::BAUD_500000},
    {576000,  LibSerial::BaudRate::BAUD_576000},
    {921600,  LibSerial::BaudRate::BAUD_921600},
    {1000000, LibSerial::BaudRate::BAUD_1000000},
    {1152000, LibSerial::BaudRate::BAUD_1152000},
    {1500000, LibSerial::BaudRate::BAUD_1500000},
    {2000000, LibSerial::BaudRate::BAUD_2000000},
    {2500000, LibSerial::BaudRate::BAUD_2500000},
    {3000000, LibSerial::BaudRate::BAUD_3000000},
    {3500000, LibSerial::BaudRate::BAUD_3500000},
    {4000000, LibSerial::BaudRate::BAUD_4000000},
};

std::optional<LibSerial::BaudRate> baudrate_to_constant(int baud)
{
    if (auto it = baud_map.find(baud); it != baud_map.end())
        return it->second;
    return std::nullopt;
}
namespace
{
    std::string getEnvString(const std::string& env, const std::string& def)
    {
        const char* env_p = std::getenv(env.c_str());
        if(env_p == nullptr)
        {
            return def;
        }
        return std::string{env_p};
    }

} // namespace utils
namespace global
{
    mandeye::GNSSClient gnssClient;
    std::string ntripHost;
    int ntripPort;
    std::string ntripMountPoint;
    std::string ntripUser;
    std::string ntripPassword;
    std::string uartPort = "/dev/ublox";

    int uartBaudRate = 115200;
    std::string directoryName = "EXTRA_GNSS";
}

namespace state
{
    std::mutex stateMutex;
    std::string modeName;
    double timestamp;
    std::string continousScanTarget;
}
void clientThread()
{
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
                std::string msg_str(static_cast<char*>(message.data()), message.size());

                nlohmann::json j = nlohmann::json::parse(msg_str);
                if (j.is_object()) {
                    std::lock_guard<std::mutex> lck(state::stateMutex);
                    if (j.contains("time"))
                    {
                        state::timestamp = j["time"].get<double>();
                        global::gnssClient.setLaserTimestamp(state::timestamp);
                    }
                    if (j.contains("mode")) {
                        state::modeName = j["mode"].get<std::string>();
                    }
                    if (j.contains("continousScanDirectory")) {
                        state::continousScanTarget = j["continousScanDirectory"].get<std::string>();
                    }
                }
            }
        }
    }
    catch (const zmq::error_t& e)
    {
        std::cerr << "ZeroMQ Error: " << e.what() << std::endl;
        std::abort();
    }
}
using namespace Pistache;
struct HelloHandler : public Http::Handler {
    HTTP_PROTOTYPE(HelloHandler)

    void onRequest(const Http::Request& request, Http::ResponseWriter writer) override {
        auto setCors = [&](Http::ResponseWriter &w) {
            w.headers().add<Http::Header::AccessControlAllowOrigin>("*");
            w.headers().add<Http::Header::AccessControlAllowMethods>("GET,POST,OPTIONS");
            w.headers().add<Http::Header::AccessControlAllowHeaders>("Content-Type");
        };

        setCors(writer);

        auto status = global::gnssClient.produceStatus();
        writer.send(Http::Code::Ok, status.dump(4), MIME(Application, Json));
        return;

    }
};

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


void NMEACallback(const std::string& nmea)
{
    namespace  fs = std::filesystem;
    static auto lastTime = std::chrono::steady_clock::now();
    static int fileCount = 0 ;
    static std::string buffer;
    const auto now = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
    static std::string lastMode = "";

    // get current mode
    std::string mode;
    std::string continousScanTarget;
    {
        std::lock_guard<std::mutex> lck(state::stateMutex);
        continousScanTarget = state::continousScanTarget;
        mode = state::modeName;
    }

    if (lastMode!= mode && mode == "SCANNING")
    {
        lastTime = std::chrono::steady_clock::now();
    }
    lastMode = mode;
    //
    if (mode == "SCANNING")
    {
        buffer.append(nmea);
    }


    if (duration.count() > 60 || mode == "STOPPING" )
    {
        if (!buffer.empty())
        {

            // construct path
            const fs::path directory = fs::path(continousScanTarget)/ global::directoryName;
            // mkdir -p
            std::filesystem::create_directories(directory);

            const auto start = std::chrono::high_resolution_clock::now();

            const auto filename = directory.string() + "/" + "extra_gnss" + std::to_string(start.time_since_epoch().count()) + ".gnss";

            std::ofstream file(filename);
            file << buffer;
            file.close();
            std::cout << "Saved file " << filename << std::endl;
            fileCount++;
            buffer.clear();
            lastTime = std::chrono::steady_clock::now();
        }
    }
}

int main(int argc, char** argv)
{
    std::cout << "Starting" << std::endl;

    int portNo = 8090;

    // load from usb

    std::string configPath = getEnvString("EXTRA_GNSS_CONFIG_PATH", "/media/usb/config_extra_gps.json");
    std::cout << "Loading configuration from usb : " << configPath << std::endl;
    global::directoryName = getEnvString("EXTRA_GNSS_DIRECTORY_NAME", "EXTRA_GNSS");
    global::uartPort = getEnvString("EXTRA_GNSS_UART_PORT", "/dev/ublox");
    global::uartBaudRate = std::stoi(getEnvString("EXTRA_GNSS_UART_BAUD_RATE", "115200"));
    nlohmann::json configJson;

    bool configOk = false;
    try
    {
        configJson = getConfig(configPath);
        if (configJson.is_object() && !configJson.empty())
        {
            std::cout << "Loaded configuration" << std::endl;
            std::cout << configJson.dump(4) << std::endl;
            global::ntripHost = configJson["ntrip"]["host"];
            global::ntripPort = configJson["ntrip"]["port"];
            global::ntripMountPoint = configJson["ntrip"]["mount_point"];
            global::ntripUser = configJson["ntrip"]["user_name"];
            global::ntripPassword = configJson["ntrip"]["password"];
            global::uartPort = configJson["uart"]["port"];
            global::uartBaudRate = configJson["uart"]["baud_rate"];
            configOk = true;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to load config: " << e.what() << std::endl;
    }
    if (!configOk)
    {
        // create avlid config
        configJson["ntrip"]["host"] = global::ntripHost;
        configJson["ntrip"]["port"] = global::ntripPort;
        configJson["ntrip"]["mount_point"] = global::ntripMountPoint;
        configJson["ntrip"]["user_name"] = global::ntripUser;
        configJson["ntrip"]["password"] = global::ntripPassword;
        configJson["uart"]["port"] = global::uartPort;
        configJson["uart"]["baud_rate"] = global::uartBaudRate;
        configJson["uart"]["port"] = global::uartPort;
        configJson["uart"]["baud_rate"] = global::uartBaudRate;
        std::ofstream configFile(configPath);
        configFile << configJson.dump(4);
        std::cout << "Created default config at " << configPath << std::endl;
    }
    std::cout << "Starting GNSS client" << std::endl;
    if (!global::ntripHost.empty() || !global::ntripMountPoint.empty())
    {

        global::gnssClient.setNtripClient(global::ntripUser, global::ntripPassword, global::ntripMountPoint,  global::ntripHost, std::to_string( global::ntripPort));
    }
    global::gnssClient.setDataCallback(NMEACallback);

    const auto baudrate = baudrate_to_constant(global::uartBaudRate);
    if (baudrate == std::nullopt)
    {
        std::cerr << "Invalid baudrate: " << global::uartBaudRate << std::endl;
        return -1;
    }

    bool isOk = global::gnssClient.startListener(global::uartPort, *baudrate);
    if (!isOk) {
        return -1;
    }
    std::thread tzmq(clientThread);


    Address addr(Ipv4::any(), Port(portNo));
    Http::Endpoint server(addr);

    // Enable ReuseAddr to allow fast restarts
    auto opts = Http::Endpoint::options()
                    .threads(1)
                    .maxRequestSize(1 * 1024 * 1024)// 16 MB
                    .flags(Tcp::Options::ReuseAddr);
    server.init(opts);

    server.setHandler(Http::make_handler<HelloHandler>());
    server.serve();

    tzmq.join();
    return 0;
}