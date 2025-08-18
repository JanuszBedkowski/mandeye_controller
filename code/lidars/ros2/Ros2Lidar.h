#pragma once

#include "lidars/BaseLidarClient.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/imu.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
namespace mandeye
{

    class ROS2Lidar : public BaseLidarClient
    {
    public:
        ROS2Lidar() = default;
        ~ROS2Lidar() override = default;

        nlohmann::json produceStatus() override;

        //! starts ROS2Lidar, interface is IP of listen interface (IP of network cards with ROS2Lidar connected
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
        // Add any private members or methods if needed
        std::mutex m_bufferImuMutex;
        LidarPointsBufferPtr m_bufferLidarPtr;
        LidarIMUBufferPtr m_bufferIMUPtr;
        std::thread m_watchThread;
        std::atomic_bool isDone{ false }; // Flag to control the data thread
        std::atomic_int m_recivedPointMessages{ 0 }; // Counter for received point messages
        std::atomic_int m_receivedImuMessages{ 0 }; // Counter for received point messages
        std::atomic<double> m_lastTimestamp {0};
        std::shared_ptr<rclcpp::Node> m_rosNode;
        std::shared_ptr<rclcpp::Subscription<sensor_msgs::msg::Imu>> m_imuSubscription;
        std::shared_ptr<rclcpp::Subscription<sensor_msgs::msg::PointCloud2>> m_pointcloudSubscription;

    };
    extern "C" void* create_client() {
        return new mandeye::ROS2Lidar();
    }

    extern "C" void destroy_client(void* ptr) {
        delete static_cast<mandeye::ROS2Lidar*>(ptr);
    }

} // namespace mandeye