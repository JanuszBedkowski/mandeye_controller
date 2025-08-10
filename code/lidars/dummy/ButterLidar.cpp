#include "lidars/dummy/ButterLidar.h"

#include <iostream>
namespace mandeye
{

    nlohmann::json ButterLidar::produceStatus()
    {
        nlohmann::json data;
        data["ButterLidar"]["status"]["init_success"] = true; // Simulate successful initialization
        data["ButterLidar"]["status"]["is_done"] = isDone.load(); // Current status of the data thread
        data["ButterLidar"]["status"]["received_point_messages"] = m_recivedPointMessages.load(); // Number of received point messages

        return data;
    }
    bool ButterLidar::startListener(const std::string& interfaceIp)
    {
        std::cout << "ButterLidar: startListener called with interfaceIp: " << interfaceIp << std::endl;
        std::lock_guard<std::mutex> lock(m_bufferImuMutex);
        // Simulate starting the listener
        m_watchThread = std::thread(&ButterLidar::DataThreadFunction, this);
        return true; // Simulate success
    }

    void ButterLidar::startLog()
    {
        std::cout << "ButterLidar: startLog called" << std::endl;
        // Initialize buffers
        std::lock_guard<std::mutex> lock(m_bufferImuMutex);
        m_bufferLidarPtr = std::make_shared<LidarPointsBuffer>();
        m_bufferIMUPtr = std::make_shared<LidarIMUBuffer>();
        // Simulate starting log
        std::cout << "ButterLidar: Logging started" << std::endl;
    }

    void ButterLidar::stopLog()
    {
        std::lock_guard<std::mutex> lock(m_bufferImuMutex);
        // Stop the data thread
        m_bufferLidarPtr.reset();
        m_bufferIMUPtr.reset();
        std::cout << "ButterLidar: stopLog called" << std::endl;
    }

    std::pair<LidarPointsBufferPtr, LidarIMUBufferPtr> ButterLidar::retrieveData()
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

    void ButterLidar::DataThreadFunction()
    {
        std::cout << "ButterLidar: DataThreadFunction started" << std::endl;
        while (!isDone)
        {
            // Simulate data processing
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::lock_guard<std::mutex> lock(m_bufferImuMutex);
            if (m_bufferLidarPtr)
            {
                // Simulate adding data to the buffer
                LidarPoint point;
                point.x = 1.0f; // Example data
                point.y = 2.0f;
                point.z = 3.0f;
                point.intensity = 100.0f;
                point.timestamp = 1234567890; // Example timestamp
                m_bufferLidarPtr->push_back(point);
            }
            if (m_bufferIMUPtr)
            {
                // Simulate adding IMU data to the buffer
                LidarIMU imuData;
                imuData.gyro_x = 0.01f; // Example data
                imuData.gyro_y = 0.02f;
                imuData.gyro_z = 0.03f;
                imuData.acc_x = 0.1f;
                imuData.acc_y = 0.2f;
                imuData.acc_z = 0.3f;
                imuData.timestamp = 1234567890; // Example timestamp
                m_bufferIMUPtr->push_back(imuData);
            }
            m_recivedPointMessages.fetch_add(1); // Increment the counter for received point messages
        }
        std::cout << "ButterLidar: DataThreadFunction ended" << std::endl;
    }
} // namespace mandeye
