#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>

#include <libcamera/libcamera.h>
#include <fstream>
#include <pistache/endpoint.h>
#include <zmq.hpp>

using namespace libcamera;
using namespace std::chrono_literals;
#include <future>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <chrono>
#include "LibCameraWrapper.h"
#include "index.html.h"

using namespace Pistache;

namespace global {
    mandeye::LibCameraWrapper cam;
    cv::Mat lastPhoto;
    nlohmann::json photoMetadata;
    std::mutex photoMutex;
    nlohmann::json loadedUSBConfig;
    std::mutex stateMutex;
    std::string state;
    std::string continousScanTarget;
    bool isContinousScanRunning() {
        std::lock_guard<std::mutex> lck(global::stateMutex);
        return global::state=="SCANNING";
    }
    std::string getContinousScanTarget() {
        std::lock_guard<std::mutex> lck(global::stateMutex);
        return global::continousScanTarget;
    }

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


template <typename T>
std::optional<T> GetValue(const Http::Request& request, const std::string& key) {
    auto query = request.query();
    if (query.has(key)) {
        try {
            return std::stof(query.get(key).value());
        } catch (const std::exception& e) {
            std::cerr << "Error parsing query parameter " << key << ": " << e.what() << std::endl;
            return std::nullopt;
        }
    }
    return std::nullopt;
}

struct HelloHandler : public Http::Handler {
    HTTP_PROTOTYPE(HelloHandler)

    void onRequest(const Http::Request& request, Http::ResponseWriter writer) override {
        auto setCors = [&](Http::ResponseWriter &w) {
            w.headers().add<Http::Header::AccessControlAllowOrigin>("*");
            w.headers().add<Http::Header::AccessControlAllowMethods>("GET,POST,OPTIONS");
            w.headers().add<Http::Header::AccessControlAllowHeaders>("Content-Type");
        };

        setCors(writer);
        if (request.resource() == "/photo") {
                std::lock_guard<std::mutex> lck(global::photoMutex);
                if (global::lastPhoto.empty()) {
                    writer.send(Http::Code::Not_Found, "No photo yet");
                    return;
                }
                cv::Mat img;
                // scale down
                cv::resize(global::lastPhoto, img, cv::Size(640, 480));
                std::vector<uchar> buf;
                cv::imencode(".jpg", img, buf);
                Http::Response response;
                writer.send(Http::Code::Ok, std::string(buf.begin(), buf.end()));
                return;
        }
        if (request.resource() == "/photoMeta") {
            std::lock_guard<std::mutex> lck(global::photoMutex);
            if (global::lastPhoto.empty()) {
                writer.send(Http::Code::Not_Found, "No photo yet");
            }
            writer.send(Http::Code::Ok, global::photoMetadata.dump(4), MIME(Application, Json));
            return;
        }
        else if (request.resource() == "/photoFull") {
            std::lock_guard<std::mutex> lck(global::photoMutex);
            if (global::lastPhoto.empty()) {
                writer.send(Http::Code::Not_Found, "No photo yet");
                return;
            }
            std::vector<uchar> buf;
            cv::imencode(".jpg", global::lastPhoto, buf);
            Http::Response response;
            writer.send(Http::Code::Ok, std::string(buf.begin(), buf.end()));
        }
        else if (request.resource() == "/getConfig") {
            auto config = global::cam.getCameraConfig();
            writer.send(Http::Code::Ok, config.dump(4), MIME(Application, Json));
            return;
        }
        else if (request.resource() == "/setConfig") {

            std::cerr << "Req method=" << request.method()
              << " resource=" << request.resource()
              << " body.len=" << request.body().size() << std::endl;

            if (request.method() == Http::Method::Post) {

                // Preflight: return CORS headers immediately, do NOT parse body
                if (request.method() == Http::Method::Options) {
                    setCors(writer);
                    writer.send(Http::Code::Ok, "OK");
                    return;
                }

                if (request.method() != Http::Method::Post) {
                    setCors(writer);
                    writer.send(Http::Code::Method_Not_Allowed, "Use POST");
                    return;
                }

                auto body = request.body(); // the raw body (string)
                std::cout << "Body: " << body << std::endl;
                if (body.empty()) {
                    writer.send(Http::Code::Bad_Request, "Empty body");
                    return;
                }
                try {
                    std::cout << "Parsing JSON" << std::endl;
                    std::cout << body << std::endl;
                    nlohmann::json config = nlohmann::json::parse(body);
                    global::cam.stop();
                    global::cam.start(config, libcamera::StreamRole::StillCapture);
                    global::cam.capture();
                    writer.send(Http::Code::Ok, "OK");
                }
                catch (const std::exception& e) {
                    std::cerr << "Error parsing JSON: " << e.what() << std::endl;
                    writer.send(Http::Code::Bad_Request, "Invalid JSON");
                }
                writer.send(Http::Code::Ok, "OK");
                return;
            }

            setCors(writer);
            writer.send(Http::Code::Method_Not_Allowed, "Use POST with JSON body");
            return;
        }
        else {
            writer.send(Http::Code::Ok, indexWebPageData.data(), MIME(Text, Html));
            return;
        }
    }
};


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



int main()
{
//
    std::cout << "Starting" << std::endl;
    // get config from USB
    const auto configFileName = getEnvString("MANDEYE_CONFIG", "/media/usb/mandeye_config.json");
    std::cout << "Loading camera config from" << configFileName << std::endl;


    // try to load config

    try {
        if (!std::filesystem::exists(configFileName)) {
            throw std::runtime_error("Config file not found on USB, will create default config");
        }
        global::loadedUSBConfig = nlohmann::json::parse(std::ifstream(configFileName));
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to load config: " << e.what() << std::endl;
    }

    std::thread t(clientThread);

    std::future<void> jpgSaveThread;

    auto printFrame = [&](cv::Mat& img, uint64_t timestamp, nlohmann::json& metaDataDump) {
        {
            std::unique_lock<std::mutex> lck(global::photoMutex);
            global::lastPhoto = std::move(img);
            global::photoMetadata = std::move(metaDataDump);
        }

        if (global::isContinousScanRunning()) {
            bool isPreviousFrameSaved = true;
            if (jpgSaveThread.valid() && jpgSaveThread.wait_for(0ms) != std::future_status::ready) {

                isPreviousFrameSaved = false;
                std::cerr << "[WARNING] Previous frame not saved yet - skipping this photo" << std::endl;
            }

            if (isPreviousFrameSaved) {
                const std::filesystem::path path(global::getContinousScanTarget());

                // delegate frame saving to std::future (separate thread).
                jpgSaveThread = std::async(std::launch::async, [=]() {
                    try {
                        const auto start = std::chrono::high_resolution_clock::now();
                        const auto filename = path.string() + "/" + "photo_" + std::to_string(timestamp) + ".jpg";;
                        // copy last photo
                        cv::Mat imgToSave;
                        {
                            std::lock_guard<std::mutex> lck(global::photoMutex);
                            imgToSave = global::lastPhoto.clone();
                        }
                        // create buffer in memory and write it
                        std::vector<uchar> buf;
                        cv::imencode(".jpg", imgToSave, buf);
                        std::ofstream file(filename, std::ios::binary);
                        file.write(reinterpret_cast<char *>(buf.data()), buf.size());
                        file.close();
                        const auto end = std::chrono::high_resolution_clock::now();
                        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                        std::cout << "Wrote " << filename << " size :" << float(buf.size()) / (1024 * 1024) << " MB in " <<
                                duration.count() << "ms" << std::endl;

                        // save metadata
                        std::ofstream metadataFile(filename + ".meta.json");
                        metadataFile << global::photoMetadata.dump(4);
                        metadataFile.close();
                        std::cout << "Wrote " << filename << ".meta.json" << std::endl;
                    } catch (const std::exception &e) {
                        std::cerr << "Failed to save photo: " << e.what() << std::endl;
                    }
                });

            }
        }
    };

    global::cam.registerCallback(printFrame);
    global::cam.start({}, libcamera::StreamRole::StillCapture);
    if (!global::loadedUSBConfig.is_object()) {
        std::cout << "No config loaded, saving default config to " << configFileName << std::endl;
        try {
            std::ofstream file(configFileName);
            file << global::cam.getCameraConfig().dump(4);
            file.close();
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to save default config: " << e.what() << std::endl;
        }
    }
    global::cam.capture(false);


    Address addr(Ipv4::any(), Port(8004));
    Http::Endpoint server(addr);

    // Enable ReuseAddr to allow fast restarts
    auto opts = Http::Endpoint::options()
                    .threads(1)
                    .maxRequestSize(16 * 1024 * 1024)// 16 MB
                    .flags(Tcp::Options::ReuseAddr);
    server.init(opts);

    server.setHandler(Http::make_handler<HelloHandler>());
    server.serve();

    global::cam.stop();
    std::cout << "Exiting" << std::endl;
    return 0;
}
