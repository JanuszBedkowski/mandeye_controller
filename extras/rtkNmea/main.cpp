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
    std::mutex stateMutex;
    std::string state;
    std::string continousScanTarget;
    mandeye::GNSSClient gnssClient;
    std::string ntripHost;
    int ntripPort;
    std::string ntripMountPoint;
    std::string ntripUser;
    std::string ntripPassword;
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
                    std::lock_guard<std::mutex> lck(global::stateMutex);
                    if (j.contains("mode")) {
                        global::state = j["mode"].get<std::string>();
                    }
                    if (j.contains("continousScanDirectory")) {
                        global::continousScanTarget = j["continousScanDirectory"].get<std::string>();
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

int main(int argc, char** argv)
{
    std::cout << "Starting" << std::endl;

    int portNo = 8090;

    // load from usb
    std::cout << "Loading configuration from usb" << std::endl;
    std::string configPath = getEnvString("CONFIG_PATH", "/mnt/usb/config.json");
    nlohmann::json configJson;
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
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to load config: " << e.what() << std::endl;
        return 1;
    }
    global::gnssClient.setNtripClient("Mpelka", "t200757p", "JOZ2_RTCM_3_2", "system.asgeupos.pl", "8086");
    std::thread tzmq(clientThread);
    std::thread tgnss([&]()
    {
        global::gnssClient.startListener("/dev/ttyACM0", LibSerial::BaudRate::BAUD_38400);
    });

    Address addr(Ipv4::any(), Port(portNo));
    Http::Endpoint server(addr);

    // Enable ReuseAddr to allow fast restarts
    auto opts = Http::Endpoint::options()
                    .threads(1)
                    .maxRequestSize(16 * 1024 * 1024)// 16 MB
                    .flags(Tcp::Options::ReuseAddr);
    server.init(opts);

    server.setHandler(Http::make_handler<HelloHandler>());
    server.serve();

    // Rest of the main function...



    tzmq.join();
    tgnss.join();
}