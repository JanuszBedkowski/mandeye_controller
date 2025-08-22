#include "lidars/ros2/Ros2Lidar.h"

#include <iostream>
#include <sensor_msgs/msg/point_field.hpp>
#include <chrono>
#include <sensor_msgs/point_cloud2_iterator.hpp>
namespace mandeye
{

    nlohmann::json ROS2Lidar::produceStatus()
    {
        nlohmann::json data;
        data["ROS2Lidar"]["status"]["init_success"] = true; // Simulate successful initialization
        data["ROS2Lidar"]["status"]["is_done"] = isDone.load(); // Current status of the data thread
        data["ROS2Lidar"]["status"]["received_point_messages"] = m_recivedPointMessages.load(); // Number of received point messages
        if (m_rosNode)
        {
            const auto clock = m_rosNode->get_clock()->now();
            data["ROS2Lidar"]["ros2"]["ros_time"]["sec"] = clock.seconds();

        }
        if (m_imuSubscription)
        {
            data["ROS2Lidar"]["ros2"]["imu_subscription"]["publisher_count"] = m_imuSubscription->get_publisher_count();
        }
        data["counters"]["imu"] = m_receivedImuMessages.load();
        data["counters"]["lidar"] = m_recivedPointMessages.load();
        data["lastLidarTimestamp"] = m_lastTimestamp.load();

        return data;
    }
    bool ROS2Lidar::startListener(const std::string& interfaceIp)
    {
        std::cout << "ROS2Lidar: startListener called with interfaceIp: " << interfaceIp << std::endl;
        std::lock_guard<std::mutex> lock(m_bufferImuMutex);
        // Simulate starting the listener
        m_watchThread = std::thread(&ROS2Lidar::DataThreadFunction, this);
        // sleep for some time
        std::this_thread::sleep_for(std::chrono::seconds(5));
        // diagnose
        if (m_recivedPointMessages==0 && m_receivedImuMessages ==0)
        {
            return false;
        }
        return true; // Simulate success
    }

    void ROS2Lidar::startLog()
    {
        std::cout << "ROS2Lidar: startLog called" << std::endl;
        // Initialize buffers
        std::lock_guard<std::mutex> lock(m_bufferImuMutex);
        m_bufferLidarPtr = std::make_shared<LidarPointsBuffer>();
        m_bufferIMUPtr = std::make_shared<LidarIMUBuffer>();
        // Simulate starting log
        std::cout << "ROS2Lidar: Logging started" << std::endl;
    }

    void ROS2Lidar::stopLog()
    {
        std::lock_guard<std::mutex> lock(m_bufferImuMutex);
        // Stop the data thread
        m_bufferLidarPtr.reset();
        m_bufferIMUPtr.reset();
        std::cout << "ROS2Lidar: stopLog called" << std::endl;
    }

    std::pair<LidarPointsBufferPtr, LidarIMUBufferPtr> ROS2Lidar::retrieveData()
    {
        std::cout << "ROS2Lidar: retrieveData called" << std::endl;
        // Return empty buffers for now

        std::lock_guard<std::mutex> lock(m_bufferImuMutex);

        LidarPointsBufferPtr returnPointerLidar{ std::make_shared<LidarPointsBuffer>() };
        LidarIMUBufferPtr returnPointerImu{ std::make_shared<LidarIMUBuffer>() };
        std::swap(m_bufferIMUPtr, returnPointerImu);
        std::swap(m_bufferLidarPtr, returnPointerLidar);
        return std::pair<LidarPointsBufferPtr, LidarIMUBufferPtr>(returnPointerLidar, returnPointerImu);
    }

    void ROS2Lidar::DataThreadFunction()
    {
        std::cout << "ROS2Lidar: DataThreadFunction started" << std::endl;
        rclcpp::init(0, nullptr);
        m_rosNode = rclcpp::Node::make_shared("ros2_lidar_node");
        m_imuSubscription = m_rosNode->create_subscription<sensor_msgs::msg::Imu>(
            "imu/data", 10,
            [this](const sensor_msgs::msg::Imu::SharedPtr msg) {
                std::lock_guard<std::mutex> lock(m_bufferImuMutex);
                if (m_bufferIMUPtr)
                {
                    LidarIMU imuData;
                    imuData.timestamp = msg->header.stamp.sec * 1e9 + msg->header.stamp.nanosec;
                    imuData.gyro_x = msg->angular_velocity.x;
                    imuData.gyro_y = msg->angular_velocity.y;
                    imuData.gyro_z = msg->angular_velocity.z;
                    imuData.acc_x = msg->linear_acceleration.x / 9.8; // Convert to g
                    imuData.acc_y = msg->linear_acceleration.y / 9.8; // Convert to g
                    imuData.acc_z = msg->linear_acceleration.z / 9.8; // Convert to g
                    imuData.laser_id = 0; // Assuming a single laser for now
                    auto now = std::chrono::system_clock::now();
                    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
                    imuData.epoch_time = millis.count();
                    m_bufferIMUPtr->emplace_back(imuData);
                }
                m_receivedImuMessages.fetch_add(1);
            });
        
        m_pointcloudSubscription = m_rosNode->create_subscription<sensor_msgs::msg::PointCloud2>(
            "/lidar_points", 10,
            [this](const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
                std::lock_guard<std::mutex> lock(m_bufferImuMutex);
                m_recivedPointMessages.fetch_add(1);

                uint64_t msg_timestamp = msg->header.stamp.sec * 1e9 + msg->header.stamp.nanosec;

                sensor_msgs::PointCloud2ConstIterator<float> iter_x(*msg, "x");
                sensor_msgs::PointCloud2ConstIterator<float> iter_y(*msg, "y");
                sensor_msgs::PointCloud2ConstIterator<float> iter_z(*msg, "z");
                sensor_msgs::PointCloud2ConstIterator<float> iter_intensity(*msg, "intensity");
                sensor_msgs::PointCloud2ConstIterator<double> iter_timestamp(*msg, "timestamp");
                for (; iter_x != iter_x.end(); ++iter_x, ++iter_y, ++iter_z, ++iter_intensity, ++iter_timestamp)
                {
                    LidarPoint point;
                    point.x = *iter_x;
                    point.y = *iter_y;
                    point.z = *iter_z;
                    point.intensity = *iter_intensity;
                    const double timestamp = iter_timestamp != iter_timestamp.end() ? (*iter_timestamp) : 0.0;
                    m_lastTimestamp.store(timestamp);
                    point.timestamp = 1e9*timestamp;
                    point.tag = 0;
                    point.line_id = 0;
                    point.laser_id = 0;
                    if (m_bufferLidarPtr)
                    {
                        m_bufferLidarPtr->emplace_back(point);
                    }
                }

                m_recivedPointMessages.fetch_add(1);
            });
        
        while (!isDone)
        {
            spin_some(m_rosNode);
        }
        std::cout << "ROS2Lidar: DataThreadFunction ended" << std::endl;
    }
} // namespace mandeye
