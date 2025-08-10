#pragma once

#include "lidars/BaseLidarClient.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <sick_scan_xd/sick_scan_api.h>
namespace mandeye
{

    class SickClient : public BaseLidarClient
    {
    public:
        SickClient() =default;
        SickClient(const SickClient&) = delete;
        SickClient& operator=(const SickClient&) = delete;
        SickClient(SickClient&&) = delete;
        ~SickClient() override = default;

        nlohmann::json produceStatus() override;

        //! starts ButterLidar, interface is IP of listen interface (IP of network cards with ButterLidar connected
        bool startListener(const std::string& interfaceIp) override;

        void stopListener() override;

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
        static void customizedPointCloudMsgCb(SickScanApiHandle apiHandle, const SickScanPointCloudMsg* msg);
        static void imuCallback(SickScanApiHandle apiHandle, const SickScanImuMsg* msg);
        static SickClient* GetInstanceFromHandle(SickScanApiHandle apiHandle);
        // Add any private members or methods if needed
        std::mutex m_bufferMutex;
        LidarPointsBufferPtr m_bufferLidarPtr;
        LidarIMUBufferPtr m_bufferIMUPtr;
        std::thread m_watchThread;
        std::atomic_bool isDone{ false }; // Flag to control the data thread
        std::atomic_int m_recivedPointMessages{ 0 }; // Counter for received point messages
        std::atomic_int m_recivedImuMessages{ 0 }; // Counter for received point messages
        int m_apiStatus { -1};
        std::string m_apiStatusMessage; // Status message from the API
        SickScanApiHandle m_apiHandle; // Handle to the Sick SDK API, replace with actual type
        static std::mutex m_instancesMutex; // Mutex to protect the static vector of instances
        static SickClient* m_instance; // Static vector to hold instances of SickClient

    };

} // namespace mandeye

extern "C" void* create_sick_client() {
    return new mandeye::SickClient();
}

extern "C" void destroy_sick_client(void* ptr) {
    delete static_cast<mandeye::SickClient*>(ptr);
}

