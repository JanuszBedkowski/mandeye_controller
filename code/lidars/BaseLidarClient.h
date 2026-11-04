#pragma once
#include "utils/TimeStampProvider.h"
#include <deque>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdint.h>
namespace mandeye
{
struct LidarPoint
{
	float x{0}; //! X coordinate in meters
	float y{0}; //! Y coordinate in meters
	float z{0}; //! Z coordinate in meters
	float intensity{0}; //! Intensity of the point, usually 0-255
	uint8_t tag{0}; //! Lidar-specific point tag (e.g. Livox tag), saved as LAZ classification
	uint64_t timestamp{0}; //! Timestamp in nanoseconds, 0 if not set
	uint8_t line_id{0}; //! Line ID, used to identify the laser that produced this point
	uint16_t laser_id{0}; //! Laser ID, used to identify the laser that produced this point
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

	//! gets a buffer size
	virtual uint64_t GetBufferSize() const
	{
		return 0;
	}
	//! Get the current mapping from serial number to lidar id
	virtual std::unordered_map<uint32_t, std::string> getSerialNumberToLidarIdMapping() const
	{
		return {};
	}

	//! Check if the lidar timer is synchronized with computer time (e.g. with PTP)
	virtual bool isSynced()
	{
		return false;
	}

	//! Check if the lidar has delivered data recently.
	//! Default is true so lidar clients without this check are not blocked.
	virtual bool isDataFlowing() const
	{
		return true;
	}
};

} // namespace mandeye