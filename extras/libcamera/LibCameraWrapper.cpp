#include "LibCameraWrapper.h"
using namespace libcamera;
using namespace std::chrono_literals;
#include <sys/mman.h>

namespace mandeye {
    uint64_t getCurrentTimestamp() {
        using namespace std::chrono;
        auto ts = duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
        return ts;
    }


    template <typename T> bool LibCameraWrapper::setControlNumeric(const std::string &name, T value) {
        for (auto const &control: m_controlsInfo) {
            if (control.first->name() == name) {
                int id = control.first->id();
                if (m_controlList.contains(id)) {
                    const int id = control.first->id();
                    const int type = control.first->type();


                    const bool isTypeOk = (type == ControlTypeFloat && std::is_floating_point<T>::value) ||
                                          (type == ControlTypeInteger32 && std::is_integral<T>::value) ||
                                          (type == ControlTypeInteger64 && std::is_integral<T>::value);

                    if (!isTypeOk) {
                        std::cerr << "Control " << name << " is not float or integer type" << std::endl;
                        throw std::runtime_error("Control type not supported");
                        return false;
                    }
                    const float minVal = control.second.min().get<T>();
                    const float maxVal = control.second.max().get<T>();
                    if (value < minVal || value > maxVal) {
                        std::cerr << "Control " << name << " value " << value << " out of range [" << minVal << ", " << maxVal << "]" << std::endl;
                        throw std::out_of_range("Value out of range");
                        return false;
                    }
                    m_controlList.set(id, ControlValue(value));
                    std::cout << "Set control " << name << " to " << value << std::endl;

                    return true;
                } else {
                    std::cerr << "Control " << name << " not found in control list" << std::endl;
                    throw std::runtime_error("Control doesn't exist");
                    return false;
                }
            }
        }
        return false;
    }



    // implementations
    template bool LibCameraWrapper::setControlNumeric<bool>(const std::string &name, bool value);
    template bool LibCameraWrapper::setControlNumeric<float>(const std::string &name, float value);
    template bool LibCameraWrapper::setControlNumeric<int>(const std::string &name, int value);


    nlohmann::json libcameraConfigToJson(const libcamera::Span<const uint8_t> &span, libcamera::ControlType type) {
        std::cout << "ControlType: " << type << " Size: " << span.size() << std::endl;
        if (type == libcamera::ControlTypeBool && span.size() == 1) {
            return span.data()[0] != 0;
        }
        else if (type == libcamera::ControlTypeByte) {
            return span.data()[0];
        }
        else if (type == libcamera::ControlTypeInteger32 && span.size() == 4) {
            int32_t val;
            memcpy(&val, span.data(), 4);
            return val;
        }
        else if (type == libcamera::ControlTypeInteger64 && span.size() == 8) {
            int64_t val;
            memcpy(&val, span.data(), 8);
            return val;
        }
        else if (type == libcamera::ControlTypeFloat && span.size() == 4) {
            float val;
            memcpy(&val, span.data(), 4);
            return val;
        }
        else if (type == libcamera::ControlTypeString) {
            return std::string(reinterpret_cast<const char *>(span.data()), span.size());
        }
        else if (type == libcamera::ControlTypeRectangle && span.size() == 16) {
            libcamera::Rectangle rect;
            memcpy(&rect, span.data(), 16);
            nlohmann::json j;
            j["x"] = rect.x;
            j["y"] = rect.y;
            j["width"] = rect.width;
            j["height"] = rect.height;
            return j;
        }
        else if (type == libcamera::ControlTypeSize && span.size() == 8) {
            libcamera::Size size;
            memcpy(&size, span.data(), 8);
            nlohmann::json j;
            j["width"] = size.width;
            j["height"] = size.height;
            return j;
        }
        else {
            // Fallback: dump as array of bytes
            nlohmann::json j = nlohmann::json::array();
            for (size_t i = 0; i < span.size(); i++)
                j.push_back(span.data()[i]);
            return j;
        }
    }


    std::vector<libcamera::Span<uint8_t> > LibCameraWrapper::Mmap(libcamera::FrameBuffer *buffer) {
        auto item = m_mapped_buffers.find(buffer);
        if (item == m_mapped_buffers.end())
            return {};
        return item->second;
    }

    std::vector<std::unique_ptr<Request> > LibCameraWrapper::CreateRequests() {
        std::vector<std::unique_ptr<Request> > requests;
        const std::vector<std::unique_ptr<libcamera::FrameBuffer> > &buffers = m_allocator->buffers(m_stream);
        for (unsigned int i = 0; i < buffers.size(); ++i) {
            std::unique_ptr<libcamera::Request> request = m_camera->createRequest();
            if (!request) {
                std::cerr << "Can't create request" << std::endl;
                return {};
            }

            const std::unique_ptr<libcamera::FrameBuffer> &buffer = buffers[i];
            int ret = request->addBuffer(m_stream, buffer.get());
            if (ret < 0) {
                std::cerr << "Can't set buffer for request"
                        << std::endl;
                return {};
            }

            requests.push_back(std::move(request));
        }
        return requests;
    }

    void LibCameraWrapper::AdjustSystemClock() {
        timespec ts_real, ts_mono;
        clock_gettime(CLOCK_REALTIME, &ts_real);
        clock_gettime(CLOCK_MONOTONIC, &ts_mono);
        // Convert to nanoseconds
        uint64_t realtime_ns = uint64_t(ts_real.tv_sec) * 1'000'000'000ULL + ts_real.tv_nsec;
        uint64_t monotonic_ns = uint64_t(ts_mono.tv_sec) * 1'000'000'000ULL + ts_mono.tv_nsec;
        assert(realtime_ns>monotonic_ns);
        m_monoOffset = uint64_t(realtime_ns) - uint64_t(monotonic_ns);
    }

    void LibCameraWrapper::start(nlohmann::json config, StreamRole role) {
        AdjustSystemClock();
        m_cm = std::make_unique<CameraManager>();
        m_cm->start();
        for (auto const &camera: m_cm->cameras())
            std::cout << camera->id() << std::endl;
        auto cameras = m_cm->cameras();
        if (cameras.empty()) {
            std::cout << "No cameras were identified on the system."
                    << std::endl;
            m_cm->stop();
            return;
        }

        std::string cameraId = cameras[0]->id();
        std::cout << "Using camera " << cameraId << std::endl;
        m_camera = m_cm->get(cameraId);
        m_camera->acquire();
        m_config = m_camera->generateConfiguration({role});

        StreamConfiguration &streamConfig = m_config->at(0);
        // streamConfig.size.width = 640;
        // streamConfig.size.height = 480;
        streamConfig.pixelFormat = libcamera::formats::BGR888;
        m_config->validate();
        std::cout << "Validated configuration is: " << streamConfig.toString() << std::endl;
        m_camera->configure(m_config.get());

        m_allocator = std::make_unique<libcamera::FrameBufferAllocator>(m_camera);


        for (libcamera::StreamConfiguration &cfg: *m_config) {
            int ret = m_allocator->allocate(cfg.stream());
            if (ret < 0) {
                std::cerr << "Can't allocate buffers" << std::endl;
            }

            for (const std::unique_ptr<libcamera::FrameBuffer> &buffer: m_allocator->buffers(cfg.stream())) {
                // "Single plane" buffers appear as multi-plane here, but we can spot them because then
                // planes all share the same fd. We accumulate them so as to mmap the buffer only once.
                size_t buffer_size = 0;
                for (unsigned i = 0; i < buffer->planes().size(); i++) {
                    const libcamera::FrameBuffer::Plane &plane = buffer->planes()[i];
                    buffer_size += plane.length;
                    if (i == buffer->planes().size() - 1 || plane.fd.get() != buffer->planes()[i + 1].fd.get()) {
                        void *memory = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, plane.fd.get(), 0);
                        m_mapped_buffers[buffer.get()].push_back(libcamera::Span<uint8_t>(
                            static_cast<uint8_t *>(memory),
                            buffer_size));
                        buffer_size = 0;
                    }
                }
            }
        }

        m_stream = streamConfig.stream();
        requests = CreateRequests();

        m_camera->requestCompleted.connect(this, &LibCameraWrapper::requestComplete);

        m_controlsInfo = m_camera->controls();
        auto configDump = getCameraConfig();
        //std::cout << configDump.dump(4) << std::endl;

        // apply config from json

        if (config.contains("picamera")) {
            const auto picamera = config["picamera"];
            for (const auto &[key, value]: picamera.items()) {
                if (key.find("_") == 0 || value.is_null()) {
                    continue;
                }
                try {
                    if (value.is_boolean()) {
                        setControlNumeric(key, value.get<bool>() ? 1.0f : 0.0f);
                    }
                    else if (value.is_number_float()) {
                        setControlNumeric(key, value.get<float>());
                    }
                    else if (value.is_number_integer()) {
                        setControlNumeric(key, static_cast<float>(value.get<int>()));
                    }
                    else {
                        std::cout << "Skipping " << key << " - unsupported type" << std::endl;
                    }
                }
                catch (const std::exception& e) {
                    std::cerr << "Failed to set " << key << ": " << e.what() << std::endl;
                }
            }
        }

        //m_controlList.set(controls::AeEnable, true);
        m_camera->start(&m_controlList);


    }


    void LibCameraWrapper::stop() {
        requests.clear();
        if (m_camera) {
            m_camera->stop();
            if (m_allocator) m_allocator->free(m_stream);
            m_camera->release();
            m_camera.reset();
            m_allocator.reset();

        }
        m_cm->stop();
        m_cm.reset();
    }

    void LibCameraWrapper::requestComplete(Request *request) {
        AdjustSystemClock();
        if (request->status() == Request::RequestCancelled)
            return;

        if (m_config->at(0).pixelFormat == libcamera::formats::MJPEG) {
            const libcamera::Request::BufferMap &buffers = request->buffers();
            for (auto bufferPair: buffers) {
                libcamera::FrameBuffer *buffer = bufferPair.second;
                libcamera::StreamConfiguration &streamConfig = m_config->at(0);
                m_requestTimestamp = buffer->metadata().timestamp + m_monoOffset;
                auto mem = Mmap(buffer);
                std::vector<uchar> memv(mem[0].begin(), mem[0].end());
                cv::Mat img = cv::imdecode(memv, cv::IMREAD_COLOR);
                if (m_callback) {
                    m_callback(img, m_requestTimestamp);
                };
            }
        } else if (m_config->at(0).pixelFormat == libcamera::formats::BGR888) {
            const libcamera::Request::BufferMap &buffers = request->buffers();
            for (auto bufferPair: buffers) {
                libcamera::FrameBuffer *buffer = bufferPair.second;
                m_requestTimestamp = buffer->metadata().timestamp + m_monoOffset;
                auto currentTime = getCurrentTimestamp();
                double exposureTime = double(currentTime) / 1e9 - double(m_requestTimestamp) / 1e9;
                std::cout << "Exposure time: " << double(currentTime) / 1e9 - double(m_requestTimestamp) / 1e9 << std::endl;
                libcamera::StreamConfiguration &streamConfig = m_config->at(0);
                unsigned int vw = streamConfig.size.width;
                unsigned int vh = streamConfig.size.height;
                unsigned int vstr = streamConfig.stride;
                auto mem = Mmap(buffer);
                cv::Mat img(vh, vw, CV_8UC3);
                uint ls = vw * 3;
                uint8_t *ptr = mem[0].data();
                for (unsigned int i = 0; i < vh; i++, ptr += vstr) {
                    memcpy(img.ptr(i), ptr, ls);
                }
                if (m_callback) {
                    m_callback(img, m_requestTimestamp);
                }
            }
        } else {
            std::cout << "Unsupported pixel format" << std::endl;
        }
        request->reuse(libcamera::Request::ReuseBuffers);

        if (!m_oneFrame) {
            m_camera->queueRequest(request);
        }
    }

    void LibCameraWrapper::capture(bool oneFrame)
    {

        m_oneFrame = oneFrame;
        for (std::unique_ptr<Request> &request: requests) {
            m_requestTimestamp = getCurrentTimestamp();
            m_camera->queueRequest(request.get());
            m_running.store(true);
        }
    }

    nlohmann::json LibCameraWrapper::getCameraConfig() {
        nlohmann::json config;
        if (!m_camera)
            return config;
        config["id"] = m_camera->id();
        libcamera::StreamConfiguration &streamConfig = m_config->at(0);
        config["buffer"]["width"] = streamConfig.size.width;
        config["buffer"]["height"] = streamConfig.size.height;

        config["buffer"]["pixelFormat"] = streamConfig.pixelFormat.toString();
        config["buffer"]["stride"] = streamConfig.stride;

        config["controls_info"] = {};
        config["picamera"]["_Note"] = "Here User can adjust setting of their camera!";
        for (auto const &control: m_controlsInfo) {
            int id = control.first->id();
            auto name = control.first->name();
            if (!name.empty()) {
                config["controls_info"][name]["name"] = control.first->name();
                config["controls_info"][name]["type"] = control.first->type();
                config["controls_info"][name]["max"] = control.second.max().toString();
                config["controls_info"][name]["min"] = control.second.min().toString();
                config["controls_info"][name]["default"] = control.second.def().toString();
                config["controls_info"][name]["values"] = control.second.values().size();
                auto type = control.first->type();
                if (type == libcamera::ControlTypeBool ||
                    type == libcamera::ControlTypeInteger32 ||
                    type == libcamera::ControlTypeInteger64 ||
                    type == libcamera::ControlTypeFloat ||
                    type == libcamera::ControlTypeByte ) {
                    config["picamera"][name] = control.second.def().toString();
                }


            }
        }
        return config;
    }


}
