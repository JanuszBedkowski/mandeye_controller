#include <zmq.hpp>
#include <pistache/endpoint.h>
#include <zmq.hpp>
#include <json.hpp>
#include "gnss.h"
#include <pistache/endpoint.h>
#include "fstream"
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
    std::string uartPort = "/dev/ttyACM0";
    int uartBaudRate = 115200;
    std::string directoryName = "extra_gnss";
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
                    if (j.contains("timestamp"))
                    {
                        state::timestamp = j["timestamp"].get<double>();
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
         throw("Config file does not exist at " + configPath);
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
    buffer.append(nmea);
    if (duration.count() > 60)
    {
        std::string continousScanTarget;
        {
            std::lock_guard<std::mutex> lck(state::stateMutex);
            continousScanTarget = state::continousScanTarget;
        }

        // construct path
        const fs::path directory = fs::path(continousScanTarget)/ global::directoryName;
        // mkdir -p
        std::filesystem::create_directories(directory);

        // save file
        char filename[100];
        sprintf(filename, "%s/%06d.nmea", directory.c_str(), fileCount);
        std::ofstream file(filename);
        file << buffer;
        file.close();
        fileCount++;
        lastTime = std::chrono::steady_clock::now();
    }

}

int main(int argc, char** argv)
{
    std::cout << "Starting" << std::endl;

    int portNo = 8090;

    // load from usb
    std::cout << "Loading configuration from usb" << std::endl;
    std::string configPath = getEnvString("CONFIG_PATH", "/mnt/usb/config_extra_gps.json");
    global::directoryName = getEnvString("DIRECTORY_NAME", "extra_gnss");
    nlohmann::json configJson;

    bool configOk = false;
    try
    {
        configJson = getConfig(configPath);
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
    }
    std::cout << "Starting GNSS client" << std::endl;
    if (!global::ntripHost.empty() || !global::ntripMountPoint.empty())
    {

        global::gnssClient.setNtripClient(global::ntripUser, global::ntripPassword, global::ntripMountPoint,  global::ntripHost, std::to_string( global::ntripPort));
    }

    // ca

    std::thread tzmq(clientThread);
    std::thread tgnss([&]()
    {
        global::gnssClient.startListener(global::uartPort, LibSerial::BaudRate(global::uartBaudRate));
    });

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
    tgnss.join();
}