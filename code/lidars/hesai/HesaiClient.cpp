#include "lidars/hesai/HesaiClient.h"

#include <iostream>

#include "hesai_lidar_sdk.hpp"
#include <memory>

namespace mandeye
{

    nlohmann::json HesaiClient::produceStatus()
    {
        nlohmann::json data;
        data["HesaiLidar"]["status"]["init_success"] = true; // Simulate successful initialization
        data["HesaiLidar"]["status"]["is_done"] = isDone.load(); // Current status of the data thread
        data["HesaiLidar"]["status"]["received_point_messages"] = m_recivedPointMessages.load(); // Number of received point messages
        data["HesaiLidar"]["status"]["last_timestamp"] = m_lastTimestampSec.load();

        return data;
    }
    bool HesaiClient::startListener(const std::string& interfaceIp)
    {
        std::cout << "ButterLidar: startListener called with interfaceIp: " << interfaceIp << std::endl;
        std::lock_guard<std::mutex> lock(m_bufferImuMutex);
        // Simulate starting the listener
        m_watchThread = std::thread(&HesaiClient::DataThreadFunction, this);
        return true; // Simulate success
    }

    void HesaiClient::startLog()
    {
        std::cout << "ButterLidar: startLog called" << std::endl;
        // Initialize buffers
        std::lock_guard<std::mutex> lock(m_bufferImuMutex);
        m_bufferLidarPtr = std::make_shared<LidarPointsBuffer>();
        m_bufferIMUPtr = std::make_shared<LidarIMUBuffer>();
        // Simulate starting log
        std::cout << "ButterLidar: Logging started" << std::endl;
    }

    void HesaiClient::stopLog()
    {
        std::lock_guard<std::mutex> lock(m_bufferImuMutex);
        // Stop the data thread
        m_bufferLidarPtr.reset();
        m_bufferIMUPtr.reset();
        std::cout << "ButterLidar: stopLog called" << std::endl;
    }

    std::pair<LidarPointsBufferPtr, LidarIMUBufferPtr> HesaiClient::retrieveData()
    {
        std::cout << "ButterLidar: retrieveData called" << std::endl;
        // Return empty buffers for now

        std::lock_guard<std::mutex> lock(m_bufferImuMutex);

        LidarPointsBufferPtr returnPointerLidar{ std::make_shared<LidarPointsBuffer>() };
        LidarIMUBufferPtr returnPointerImu{ std::make_shared<LidarIMUBuffer>() };
        std::swap(m_bufferIMUPtr, returnPointerImu);
        std::swap(m_bufferLidarPtr, returnPointerLidar);
        return std::pair<LidarPointsBufferPtr, LidarIMUBufferPtr>(returnPointerLidar, returnPointerImu);
    }

    void HesaiClient::DataThreadFunction()
    {

        HesaiLidarSdk<LidarPointXYZICRT> sdk;
        DriverParam param;
        param.use_gpu = false;
        param.input_param.source_type = DATA_FROM_LIDAR;
        param.input_param.device_ip_address = "192.168.1.201";  // lidar ip
        param.input_param.ptc_port = 9347; // lidar ptc port
        param.input_param.udp_port = 2368; // point cloud destination port
        param.input_param.multicast_ip_address = "255.255.255.255"; // multicast ip
        sdk.Init(param);
        sdk.RegRecvCallback([this](const LidarDecodedFrame<LidarPointXYZICRT>&frame)
        {
            m_recivedPointMessages++;
            std::cout << "ButterLidar: Received point cloud frame with " << frame.points_num << " points" << std::endl;
            for (int i =0; i < frame.points_num; i++)
            {
                const auto &point = frame.points[i];
                const auto &intensity = point.intensity;
                const auto &timestamp = point.timestamp;
                m_lastTimestampSec = timestamp;
                if (m_bufferLidarPtr)
                {
                    // Simulate adding data to the buffer
                    LidarPoint ppoint;
                    ppoint.x = point.x;
                    ppoint.y = point.y;
                    ppoint.z = point.z;
                    ppoint.intensity = intensity;
                    ppoint.timestamp = timestamp;
                    m_bufferLidarPtr->push_back(ppoint);
                }
            }

        });

        sdk.Start();
        while (!isDone)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        sdk.Stop();
        std::cout << "ButterLidar: DataThreadFunction started" << std::endl;

        std::cout << "ButterLidar: DataThreadFunction ended" << std::endl;
    }
} // namespace mandeye
