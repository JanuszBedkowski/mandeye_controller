#pragma once

#include "livox_lidar_def.h"
#include <deque>
#include <json.hpp>
#include <mutex>
#include "utils/TimeStampProvider.h"
namespace mandeye
{
struct LivoxPoint
{
	LivoxLidarCartesianHighRawPoint point;
	uint64_t timestamp;
	uint8_t line_id;
};

struct LivoxIMU
{
	LivoxLidarImuRawPoint point;
	uint64_t timestamp;
};

using LivoxPointsBuffer = std::deque<LivoxPoint>;
using LivoxPointsBufferPtr = std::shared_ptr<std::deque<LivoxPoint>>;
using LivoxPointsBufferConstPtr = std::shared_ptr<const std::deque<LivoxPoint>>;

using LivoxIMUBuffer = std::deque<LivoxIMU>;
using LivoxIMUBufferPtr = std::shared_ptr<std::deque<LivoxIMU>>;
using LivoxIMUBufferConstPtr = std::shared_ptr<const std::deque<LivoxIMU>>;

class LivoxClient : public mandeye_utils::TimeStampProvider
{
public:
	nlohmann::json produceStatus();

	//! starts LivoxSDK2, interface is IP of listen interface (IP of network cards with Livox connected
	bool startListener(const std::string& interfaceIp);

	//! Start log to memory data from Lidar and IMU
	void startLog();

	//! Stops log to memory data from Lidar and IMU
	void stopLog();

	std::pair<LivoxPointsBufferPtr, LivoxIMUBufferPtr> retrieveData();

	// mandeye_utils::TimeStampProvider overrides ...
	double getTimestamp() override;

private:
	std::mutex m_bufferImuMutex;
	std::mutex m_bufferLidarMutex;

	LivoxPointsBufferPtr m_bufferLivoxPtr{nullptr};
	LivoxIMUBufferPtr m_bufferIMUPtr{nullptr};

	std::mutex m_timestampMutex;
	uint64_t m_timestamp;

	uint64_t m_recivedImuMsgs{0};
	uint64_t m_recivedPointMessages{0};
	LivoxLidarInfo m_LivoxLidarInfo;
	LivoxLidarInfo m_LivoxLidarDeviceType;

	bool init_succes{false};

	static constexpr char config[] = {"{\n"
									  "\t\"MID360\": {\n"
									  "\t\t\"lidar_net_info\" : {\n"
									  "\t\t\t\"cmd_data_port\": 56100,\n"
									  "\t\t\t\"push_msg_port\": 56200,\n"
									  "\t\t\t\"point_data_port\": 56300,\n"
									  "\t\t\t\"imu_data_port\": 56400,\n"
									  "\t\t\t\"log_data_port\": 56500\n"
									  "\t\t},\n"
									  "\t\t\"host_net_info\" : {\n"
									  "\t\t\t\"cmd_data_ip\" : \"${HOSTIP}\",\n"
									  "\t\t\t\"cmd_data_port\": 56101,\n"
									  "\t\t\t\"push_msg_ip\": \"${HOSTIP}\",\n"
									  "\t\t\t\"push_msg_port\": 56201,\n"
									  "\t\t\t\"point_data_ip\": \"${HOSTIP}\",\n"
									  "\t\t\t\"point_data_port\": 56301,\n"
									  "\t\t\t\"imu_data_ip\" : \"${HOSTIP}\",\n"
									  "\t\t\t\"imu_data_port\": 56401,\n"
									  "\t\t\t\"log_data_ip\" : \"${HOSTIP}\",\n"
									  "\t\t\t\"log_data_port\": 56501\n"
									  "\t\t}\n"
									  "\t}\n"
									  "}\t"};

	// callbacks
	static void PointCloudCallback(uint32_t handle,
								   const uint8_t dev_type,
								   LivoxLidarEthernetPacket* data,
								   void* client_data);

	static void ImuDataCallback(uint32_t handle,
								const uint8_t dev_type,
								LivoxLidarEthernetPacket* data,
								void* client_data);

	static void WorkModeCallback(livox_status status,
								 uint32_t handle,
								 LivoxLidarAsyncControlResponse* response,
								 void* client_data);

	static void SetIpInfoCallback(livox_status status,
								  uint32_t handle,
								  LivoxLidarAsyncControlResponse* response,
								  void* client_data);

	static void RebootCallback(livox_status status,
							   uint32_t handle,
							   LivoxLidarRebootResponse* response,
							   void* client_data);

	static void QueryInternalInfoCallback(livox_status status,
										  uint32_t handle,
										  LivoxLidarDiagInternalInfoResponse* packet,
										  void* client_data);

	static void
	LidarInfoChangeCallback(const uint32_t handle, const LivoxLidarInfo* info, void* client_data);
};
} // namespace mandeye