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
    int cameraNo = 0;
    std::string prefix = "cam0_";
    std::string configFileName = "/media/usb/cam0_config.json";
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
                    global::cam.start(global::cameraNo, config, libcamera::StreamRole::StillCapture);
                    global::cam.capture();
                    writer.send(Http::Code::Ok, "OK");
                }
                catch (const std::exception& e) {
                    std::cerr << "Error parsing JSON: " << e.what() << std::endl;
                    writer.send(Http::Code::Bad_Request, "Invalid JSON");
                }
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



int main(int argc, char** argv)
{
    std::cout << "Starting" << std::endl;

    global::cameraNo = 0;
    int portNo = 8004;
    bool skipConfig = false;

    // Simple CLI parsing: --camera / -c, --port / -p, --config / -f, --help / -h
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--camera" || arg == "-c") && i + 1 < argc) {
            try { global::cameraNo = std::stoi(argv[++i]); } catch(...) { /* ignore on parse error */ }
        } else if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            try { portNo = std::stoi(argv[++i]); } catch(...) { /* ignore on parse error */ }
        } else if ((arg == "--prefix" || arg =="-t") && i + 1 < argc) {
            global::prefix = argv[++i];
        } else if ((arg == "--config" || arg == "-f") && i + 1 < argc) {
            global::configFileName = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [--camera <n>] [--port <n>] [--config <path>]\n";
            std::cout << "       " << argv[0] << " [--skip-config]\n";
            return 0;
        }
    }

    std::cout << "Using cameraNo=" << global::cameraNo << " port=" << portNo << " config=" << global::configFileName << std::endl;
    std::cout << "Loading camera config from " << global::configFileName << std::endl;


    // try to load config

    try {
        if (!std::filesystem::exists(global::configFileName)) {
            throw std::runtime_error("Config file not found on USB, will create default config");
        }
        global::loadedUSBConfig = nlohmann::json::parse(std::ifstream(global::configFileName));
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to load config: " << e.what() << std::endl;
    }

    // check if disabled
    if (global::loadedUSBConfig.is_object()) {
        if (global::loadedUSBConfig.contains("disabled")) {
            if (global::loadedUSBConfig["disabled"].get<bool>()) {
                std::cout << "Camera disabled, sleeping forever" << std::endl;
                while (true) {
                    std::this_thread::sleep_for(10s);
                }
                return 0;
            }
        }
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
                const std::filesystem::path continousPath(global::getContinousScanTarget());
                const std::filesystem::path directory = continousPath/ ("CAMERA_"+std::to_string(global::cameraNo));
                // mkdir, ok
                std::filesystem::create_directories(directory);

                // delegate frame saving to std::future (separate thread).
                jpgSaveThread = std::async(std::launch::async, [=]() {
                    try {
                        const auto start = std::chrono::high_resolution_clock::now();
                        const auto filename = directory.string() + "/" + global::prefix + std::to_string(timestamp);
                        const auto filenameJpg = filename+ ".jpg";
                        const auto filenameMeta = filename + ".meta.json";
                        // copy last photo
                        cv::Mat imgToSave;
                        {
                            std::lock_guard<std::mutex> lck(global::photoMutex);
                            imgToSave = global::lastPhoto.clone();
                        }
                        // create buffer in memory and write it
                        std::vector<uchar> buf;
                        cv::imencode(".jpg", imgToSave, buf);
                        std::ofstream file(filenameJpg, std::ios::binary);
                        file.write(reinterpret_cast<char *>(buf.data()), buf.size());
                        file.close();
                        const auto end = std::chrono::high_resolution_clock::now();
                        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                        std::cout << "Wrote " << filenameJpg << " size :" << float(buf.size()) / (1024 * 1024) << " MB in " <<
                                duration.count() << "ms" << std::endl;

                        // save metadata
                        std::ofstream metadataFile(filenameMeta);
                        metadataFile << global::photoMetadata.dump(4);
                        metadataFile.close();
                        std::cout << "Wrote " << filenameMeta << std::endl;
                    } catch (const std::exception &e) {
                        std::cerr << "Failed to save photo: " << e.what() << std::endl;
                    }
                });

            }
        }
    };

    global::cam.registerCallback(printFrame);
    const bool stated = global::cam.start(global::cameraNo, global::loadedUSBConfig, libcamera::StreamRole::StillCapture);
    if (!stated) {
        std::cerr << "Failed to start camera" << std::endl;
        return 1;
    }
    if (!global::loadedUSBConfig.is_object()) {
        std::cout << "No config loaded, saving default config to " << global::configFileName << std::endl;
        try {
            std::ofstream file(global::configFileName);
            file << global::cam.getCameraConfig().dump(4);
            file.close();
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to save default config: " << e.what() << std::endl;
        }
    }
    global::cam.capture(false);


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

    global::cam.stop();
    std::cout << "Exiting" << std::endl;
    return 0;
}
