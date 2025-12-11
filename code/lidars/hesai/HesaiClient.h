#pragma once

#include "lidars/BaseLidarClient.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

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
        bool isSynced() override {return true;}
    private:
        void DataThreadFunction();
        // Add any private members or methods if needed
        std::mutex m_bufferImuMutex;
        LidarPointsBufferPtr m_bufferLidarPtr;
        LidarIMUBufferPtr m_bufferIMUPtr;
        std::thread m_watchThread;
        std::atomic_bool isDone{ false }; // Flag to control the data thread
        std::atomic_int m_recivedPointMessages{ 0 }; // Counter for received point messages
        std::atomic<double> m_lastTimestampSec{ 0.0 };

    };

} // namespace mandeye

extern "C" void* create_hesai_client() {
    return new mandeye::HesaiClient();
}

extern "C" void destroy_hesai_client(void* ptr) {
    delete static_cast<mandeye::HesaiClient*>(ptr);
}
