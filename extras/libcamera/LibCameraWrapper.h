
#pragma once
#include <libcamera/libcamera.h>
#include <opencv2/opencv.hpp>
#include <atomic>
#include "json.hpp"
namespace mandeye
{

    uint64_t getCurrentTimestamp();

    const std::unordered_map<libcamera::ControlType, std::string> LibCameraControlTypeToString = {
        {libcamera::ControlType::ControlTypeNone, "ControlTypeNone"},
        {libcamera::ControlType::ControlTypeBool, "ControlTypeBool"},
        {libcamera::ControlType::ControlTypeByte, "ControlTypeByte"},
        {libcamera::ControlType::ControlTypeInteger32, "ControlTypeInteger32"},
        {libcamera::ControlType::ControlTypeInteger64, "ControlTypeInteger64"},
        {libcamera::ControlType::ControlTypeFloat, "ControlTypeFloat"},
        {libcamera::ControlType::ControlTypeString, "ControlTypeString"},
        {libcamera::ControlType::ControlTypeRectangle, "ControlTypeRectangle"},
        {libcamera::ControlType::ControlTypeSize, "ControlTypeSize"}
    };

    class LibCameraWrapper
    {
    private:
        void requestComplete(libcamera::Request *request);
        std::function<void(cv::Mat& img, uint64_t timestamp)> m_callback;
        std::shared_ptr<libcamera::Camera> m_camera;
        std::unique_ptr<libcamera::CameraManager> m_cm;
        std::unique_ptr<libcamera::FrameBufferAllocator> m_allocator;
        std::map<libcamera::FrameBuffer *, std::vector<libcamera::Span<uint8_t>>> m_mapped_buffers;
        std::unique_ptr<libcamera::CameraConfiguration> m_config;
        libcamera::Stream *m_stream = nullptr;
        std::vector<libcamera::Span<uint8_t>> Mmap(libcamera::FrameBuffer *buffer);
        std::vector<std::unique_ptr<libcamera::Request>> requests;
        libcamera::ControlInfoMap m_controlsInfo;
        libcamera::ControlList m_controlList;
        uint64_t m_requestTimestamp = 0;
        uint64_t m_monoOffset = 0;
        bool m_oneFrame = false;
        std::atomic<bool> m_running = false;
        //! Create requests and associate buffers to them
        std::vector<std::unique_ptr<libcamera::Request>> CreateRequests();

        //! Adjust the system clock offset based on monotonic clock to @m_monoOffset
        void AdjustSystemClock();

    public:

        LibCameraWrapper() = default;
        ~LibCameraWrapper() = default;
        void start(nlohmann::json config = {} ,libcamera::StreamRole role = libcamera::StreamRole::StillCapture);
        void capture(bool oneFrame = false);
        void stop();
        void registerCallback(std::function<void(cv::Mat& img, uint64_t timestamp)> cb) { m_callback = cb; }
        nlohmann::json getCameraConfig();
        libcamera::ControlList& getControlList() { return m_controlList; }
        template <typename T> bool setControlNumeric(const std::string &name, T value);



    };

}