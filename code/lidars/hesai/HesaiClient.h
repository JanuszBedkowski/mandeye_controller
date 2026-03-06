#pragma once

#include "lidars/BaseLidarClient.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

// hesai sdk
#include "hesai_lidar_sdk.hpp"

namespace mandeye
{

    class HesaiClient : public BaseLidarClient
    {
    public:
        HesaiClient() = default;
        ~HesaiClient() override = default;

        nlohmann::json produceStatus() override;

        //! starts ButterLidar, interface is IP of listen interface (IP of network cards with ButterLidar connected
        bool startListener(const std::string& interfaceIp) override;

        //! Start log to memory data from Lidar and IMU
        void startLog() override;

        //! Stops log to memory data from Lidar and IMU
        void stopLog() override;

        std::pair<LidarPointsBufferPtr, LidarIMUBufferPtr> retrieveData() override;

        // TimestampProvider overrides ...
        double getTimestamp() override
        {
            return 0.0;
        } // Dummy implementation
        double getSessionDuration() override
        {
            return 0.0;
        } // Dummy implementation
        double getSessionStart() override
        {
            return 0.0;
        } // Dummy implementation
        void initializeDuration() override
        {
        } // Dummy implementation

    private:
        void DataThreadFunction();
        void CallbackFrame(const LidarDecodedFrame<LidarPointXYZICRT>& dataFrame);
        void CallbackIMU(const LidarImuData& dataFrame);
        void CallbackFault(const FaultMessageInfo& fault_message_info);

        // Add any private members or methods if needed
        std::mutex m_bufferImuMutex;
        std::mutex m_bufferPointMutex;
        LidarPointsBufferPtr m_bufferLidarPtr;
        LidarIMUBufferPtr m_bufferIMUPtr;
        std::thread m_watchThread;
        std::atomic_bool isDone{ false }; // Flag to control the data thread
        std::atomic_int m_recivedPointMessages{ 0 }; // Counter for received point messages
        std::atomic_int m_recivedIMUMessages{ 0 }; // Counter for received IMU messages
        std::unique_ptr<HesaiLidarSdk<LidarPointXYZICRT>> m_lidar;
        std::deque<FaultMessageInfo> m_faults;
        int16_t m_lidar_state;
        int16_t m_work_mode;
        uint16_t m_packet_num;
        std::string m_software_vers;
        std::string m_hardware_vers;
        uint16_t m_laser_num;
        uint16_t m_channel_num;
        double m_timestamp;
    };

} // namespace mandeye

extern "C" void* create_client() {
    return new mandeye::HesaiClient();
}

extern "C" void destroy_client(void* ptr) {
    delete static_cast<mandeye::HesaiClient*>(ptr);
}
