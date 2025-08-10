#pragma once
#include "utils/TimeStampProvider.h"
#include <deque>
#include <json.hpp>
#include <memory>
#include <stdint.h>
namespace mandeye
{
    struct LidarPoint
    {
        float x; //! X coordinate in meters
        float y; //! Y coordinate in meters
        float z; //! Z coordinate in meters
        float intensity; //! Intensity of the point, usually 0-255
        uint8_t tag;
        uint64_t timestamp; //! Timestamp in nanoseconds, 0 if not set
        uint8_t line_id; //! Line ID, used to identify the laser that produced this point
        uint16_t laser_id; //! Laser ID, used to identify the laser that produced this point
    };

    struct LidarIMU
    {
        float gyro_x;
        float gyro_y;
        float gyro_z;
        float acc_x;
        float acc_y;
        float acc_z;

        uint64_t timestamp;
        uint16_t laser_id;
        uint64_t epoch_time;
    };

    using LidarPointsBuffer = std::deque<LidarPoint>;
    using LidarPointsBufferPtr = std::shared_ptr<std::deque<LidarPoint>>;
    using LidarPointsBufferConstPtr = std::shared_ptr<const std::deque<LidarPoint>>;

    using LidarIMUBuffer = std::deque<LidarIMU>;
    using LidarIMUBufferPtr = std::shared_ptr<std::deque<LidarIMU>>;
    using LidarIMUBufferConstPtr = std::shared_ptr<const std::deque<LidarIMU>>;

    using BaseLidarClientPtr = std::shared_ptr<class BaseLidarClient>;
    using BaseLidarClientConstPtr = std::shared_ptr<const class BaseLidarClient>;

    class BaseLidarClient : public mandeye_utils::TimeStampProvider
    {
    public:
        //! Initialize the client with a configuration
        virtual void Init(const nlohmann::json& config) {};
        virtual ~BaseLidarClient() = default;

        //! Produce a status report in JSON format
        virtual nlohmann::json produceStatus() = 0;

        //! Start the listener on a specific interface IP
        virtual bool startListener(const std::string& interfaceIp) = 0;

        //! Stops the listener on a specific interface IP
        virtual void stopListener() {};

        //! Start logging data from the Lidar and IMU
        virtual void startLog() = 0;

        //! Stop logging data from the Lidar and IMU
        virtual void stopLog() = 0;

        //! Move the data from the internal buffers to the caller, preparing new buffers
        virtual std::pair<LidarPointsBufferPtr, LidarIMUBufferPtr> retrieveData() = 0;

        //! Get the current mapping from serial number to lidar id
        virtual std::unordered_map<uint32_t, std::string> getSerialNumberToLidarIdMapping() const
        {
            return {};
        }
    };

} // namespace mandeye