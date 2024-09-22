#pragma once
#include "LivoxTypes.h"
#include "livox_def.h"
#include "livox_sdk.h"
#include <mutex>
#include "utils/TimeStampProvider.h"
namespace mandeye
{

class LivoxLegacyClient : public mandeye_utils::TimeStampProvider
{
public:
	nlohmann::json produceStatus();


	std::pair<LivoxPointsBufferPtr, LivoxIMUBufferPtr> retrieveData();

	void startLog();

	void stopLog();

	bool Initialize();

	// mandeye_utils::TimeStampProvider overrides ...
	double getTimestamp() override;

	//! Return current mapping from serial number to lidar id
	std::unordered_map<uint32_t, std::string> getSerialNumberToLidarIdMapping() const;

private:
	uint64_t m_timestamp;
	uint64_t m_recivedImuMsgs{0};
	uint64_t m_recivedPointMessages{0};
	static std::mutex m_bufferImuMutex;
	static std::mutex m_bufferLidarMutex;
	static std::mutex m_lastTimestampMutex;
	static std::mutex m_lidarIdToHandleMappingMutex;

	static double m_LastTimestamp;
	static LivoxPointsBufferPtr m_bufferLivoxPtr;
	static LivoxIMUBufferPtr m_bufferIMUPtr;


	enum DeviceStateType
	{
		kDeviceStateDisconnect = 0,
		kDeviceStateConnect = 1,
		kDeviceStateSampling = 2,
	} ;

	struct DeviceItemType
	{
		uint8_t handle;
		DeviceStateType device_state;
		DeviceInfo info;
	} ;

	static DeviceItemType devices[kMaxLidarCount];
	static uint32_t data_recveive_count[kMaxLidarCount];
	static uint32_t imu_recveive_count[kMaxLidarCount];

	static std::unordered_map<uint32_t, std::string> m_handleToSerialNumber;


	/** Connect all the broadcast device. */
	static int lidar_count;
	static char broadcast_code_list[kMaxLidarCount][kBroadcastCodeSize];

	//! starts LivoxSDK2, interface is IP of listen interface (IP of network cards with Livox connected
	static bool startListener(const std::string& interfaceIp);

	/** Receiving error message from Livox Lidar. */
	static void OnLidarErrorStatusCallback(livox_status status, uint8_t handle, ErrorMessage* message);

	/** Receiving point cloud data from Livox LiDAR. */
	static void GetLidarData(uint8_t handle, LivoxEthPacket* data, uint32_t data_num, void* client_data);

	/** Callback function of starting sampling. */
	static void OnSampleCallback(livox_status status, uint8_t handle, uint8_t response, void* data);

	/** Query the firmware version of Livox LiDAR. */
	static void OnDeviceInformation(livox_status status, uint8_t handle, DeviceInformationResponse* ack, void* data);

	static void LidarConnect(const DeviceInfo* info);

	static void LidarDisConnect(const DeviceInfo* info);

	static void LidarStateChange(const DeviceInfo* info);

	static void OnDeviceInfoChange(const DeviceInfo* info, DeviceEvent type);

	/** Callback function of stopping sampling. */
	static void OnStopSampleCallback(livox_status status, uint8_t handle, uint8_t response, void* data);

	/** Query the firmware version of Livox LiDAR. */
	static void DeviceInformation(livox_status status, uint8_t handle, DeviceInformationResponse* ack, void* data);

	/** Callback function when broadcast message received.
 * You need to add listening device broadcast code and set the point cloud data callback in this function.
 */
	static void OnDeviceBroadcast(const BroadcastDeviceInfo* info);



};

} // namespace mandeye
