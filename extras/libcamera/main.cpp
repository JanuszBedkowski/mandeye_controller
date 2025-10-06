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


using namespace Pistache;
mandeye::LibCameraWrapper cam;

cv::Mat lastPhoto;
std::mutex photoMutex;

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
        if (request.resource() == "/photo") {
                //cam.capture(true);
                while (lastPhoto.empty()) {
                    std::this_thread::sleep_for(100ms);
                }

                std::vector<uchar> buf;
                cv::imencode(".jpg", lastPhoto, buf);
                Http::Response response ;//= Http::Response::fromData("image/jpeg", );
                writer.send(Http::Code::Ok, std::string(buf.begin(), buf.end()));
                return;
        }
        else if (request.resource() == "/config") {
            auto config = cam.getCameraConfig();
            writer.send(Http::Code::Ok, config.dump(4), MIME(Application, Json));
            return;
        }
        else if (request.resource() == "/set") {
            // get gain from query
            auto analogueGain = GetValue<float>(request, "AnalogueGain");
            auto analogueGainMode = GetValue<int>(request, "AnalogueGainMode");
            auto exposureTime = GetValue<float>(request, "ExposureTime");
            auto aeEnable = GetValue<bool>(request, "AeEnable");
            auto awbEnable = GetValue<bool>(request, "AwbEnable");
            auto brightness = GetValue<float>(request, "Brightness");
            auto contrast = GetValue<float>(request, "Contrast");
            auto saturation = GetValue<float>(request, "Saturation");
            auto sharpness = GetValue<float>(request, "Sharpness");
            auto hdrMode = GetValue<float>(request, "HdrMode");
            if (analogueGain) cam.getControlList().set(controls::AnalogueGain, *analogueGain);
            if (exposureTime) cam.getControlList().set(controls::ExposureTime, int32_t(*exposureTime));
            if (aeEnable) cam.getControlList().set(controls::AeEnable, *aeEnable != 0.0f);
            if (awbEnable) cam.getControlList().set(controls::AwbEnable, *awbEnable != 0.0f);
            if (brightness) cam.getControlList().set(controls::Brightness, *brightness);
            if (contrast) cam.getControlList().set(controls::Contrast, *contrast);
            if (saturation) cam.getControlList().set(controls::Saturation, *saturation);
            if (sharpness) cam.getControlList().set(controls::Sharpness, *sharpness);


            cam.stop();
            cam.start({}, libcamera::StreamRole::Viewfinder);
            writer.send(Http::Code::Ok, "OK");
            return;
        }
        else {
            writer.send(Http::Code::Not_Found, "Not Found");
            return;
        }
    }
};

namespace globals {
    std::mutex stateMutex;
    std::string state;
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
                    std::lock_guard<std::mutex> lck(globals::stateMutex);
                    if (j.contains("mode")) {
                        globals::state = j["mode"].get<std::string>();
                    }
                    if (j.contains("continousScanDirectory")) {
                        globals::continousScanTarget = j["continousScanDirectory"].get<std::string>();
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
    // get config from

    std::thread t(clientThread);

    std::future<void> jpgSaveThread;

    auto printFrame = [&](cv::Mat& img, uint64_t timestamp) {
        lastPhoto = img;
        //
        std::lock_guard<std::mutex> lck(globals::stateMutex);
        bool isPreviousFrameSaved = true;
        if (jpgSaveThread.valid() && jpgSaveThread.wait_for(0ms) != std::future_status::ready) {
            isPreviousFrameSaved = false;
            std::cerr << "[WARNING] Previous frame not saved yet" << std::endl;
        }

        if (isPreviousFrameSaved) {
            const std::filesystem::path path(globals::continousScanTarget);
            jpgSaveThread = std::async(std::launch::async, [=]() {
                const auto start = std::chrono::high_resolution_clock::now();
                const auto filename = path.string() + "/" + "photo_" + std::to_string(timestamp) + ".jpg";;
            // cv::imwrite(filename.c_str(), img);
            // create buffer in memory and write it
            std::vector<uchar> buf;
            cv::imencode(".jpg", img, buf);
            std::ofstream file(filename, std::ios::binary);
            file.write(reinterpret_cast<char*>(buf.data()), buf.size());
            file.close();
                const auto end = std::chrono::high_resolution_clock::now();
                const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            std::cout << "Wrote " << filename << " size :" << float(buf.size())/(1024*1024) << " in " << duration.count() << "ms" << std::endl;
            });
        }
    };

    cam.registerCallback(printFrame);
    cam.start({}, libcamera::StreamRole::StillCapture);
    cam.capture(false);


    Address addr(Ipv4::any(), Port(8004));
    Http::Endpoint server(addr);

    // Enable ReuseAddr to allow fast restarts
    auto opts = Http::Endpoint::options()
                    .threads(1)
                    .flags(Tcp::Options::ReuseAddr);
    server.init(opts);

    server.setHandler(Http::make_handler<HelloHandler>());
    server.serve();

    cam.stop();
    std::cout << "Exiting" << std::endl;
    return 0;
}
