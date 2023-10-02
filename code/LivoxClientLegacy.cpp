#include "LivoxClientLegacy.h"
#include <iostream>
namespace mandeye
{

namespace {
typedef struct {
	uint32_t points_per_second; /**< number of points per second */
	uint32_t point_interval;    /**< unit:ns */
	uint32_t line_num;          /**< laser line number */
} ProductTypePointInfoPair;


const uint32_t kMaxProductType = 9;

const ProductTypePointInfoPair product_type_info_pair_table[kMaxProductType] = {
	{100000, 10000, 1},
	{100000, 10000, 1},
	{240000, 4167 , 6}, /**< tele */
	{240000, 4167 , 6},
	{100000, 10000, 1},
	{100000, 10000, 1},
	{100000, 10000, 1}, /**< mid70 */
	{240000, 4167,  6},
	{240000, 4167,  6},
};
union ToUint64
{
	uint64_t data;
	uint8_t array[8];
};

}

LivoxLegacyClient::DeviceItemType LivoxLegacyClient::devices[kMaxLidarCount];
uint32_t LivoxLegacyClient::data_recveive_count[kMaxLidarCount];
uint32_t LivoxLegacyClient::imu_recveive_count[kMaxLidarCount];

int LivoxLegacyClient::lidar_count = 0;
char LivoxLegacyClient::broadcast_code_list[kMaxLidarCount][kBroadcastCodeSize];

std::mutex LivoxLegacyClient::m_bufferImuMutex;
std::mutex LivoxLegacyClient::m_bufferLidarMutex;
std::mutex LivoxLegacyClient::m_lastTimestampMutex;

double LivoxLegacyClient::m_LastTimestamp=0;
LivoxPointsBufferPtr LivoxLegacyClient::m_bufferLivoxPtr;
LivoxIMUBufferPtr LivoxLegacyClient::m_bufferIMUPtr;


bool LivoxLegacyClient::Initialize()
{
	lidar_count = 0;
	if(!Init())
	{
		return false;
	}

	printf("Livox SDK has been initialized.\n");

	LivoxSdkVersion _sdkversion;
	GetLivoxSdkVersion(&_sdkversion);
	printf("Livox SDK version %d.%d.%d .\n", _sdkversion.major, _sdkversion.minor, _sdkversion.patch);

	memset(devices, 0, sizeof(devices));
	memset(data_recveive_count, 0, sizeof(data_recveive_count));
	memset(imu_recveive_count, 0, sizeof(imu_recveive_count));


	SetBroadcastCallback(OnDeviceBroadcast);

	SetDeviceStateUpdateCallback(OnDeviceInfoChange);

	if(!Start())
	{
		Uninit();
		return false;
	}

	return true;
}

nlohmann::json LivoxLegacyClient::produceStatus()
{
	nlohmann::json status;
	status["lidar_count"] = lidar_count;
	int points = 0;
	int imu = 0;
	for(int i = 0; i < lidar_count; i++)
	{
		points += data_recveive_count[i];
		imu += imu_recveive_count[i];
		status["lidars"][i]["broadcast_code"] = broadcast_code_list[i];
		status["lidars"][i]["data_recveive_count"] = data_recveive_count[i];
		status["lidars"][i]["device_state"] = devices[i].device_state;
		status["lidars"][i]["info"]["broadcast_code"] = devices[i].info.broadcast_code;
		status["lidars"][i]["info"]["id"] = devices[i].info.id;
		status["lidars"][i]["info"]["ip"] = devices[i].info.ip;
		status["lidars"][i]["info"]["cmd_port"] = devices[i].info.cmd_port;
		status["lidars"][i]["info"]["data_port"] = devices[i].info.data_port;
		status["lidars"][i]["info"]["slot"] = devices[i].info.slot;
		status["lidars"][i]["info"]["state"] = devices[i].info.state;
		status["lidars"][i]["info"]["type"] = devices[i].info.type;
		status["lidars"][i]["info"]["firmware_version"] = devices[i].info.firmware_version;

	}
	status["counters"]["lidar"] = points;
	status["counters"]["imu"] = imu;
	status["timestamp"] = m_LastTimestamp;
	return status;
}

/** Receiving error message from Livox Lidar. */
void LivoxLegacyClient::OnLidarErrorStatusCallback(livox_status status, uint8_t handle, ErrorMessage* message)
{
	static uint32_t error_message_count = 0;
	if(message != NULL)
	{
		++error_message_count;
		if(0 == (error_message_count % 100))
		{
			printf("handle: %u\n", handle);
			printf("temp_status : %u\n", message->lidar_error_code.temp_status);
			printf("volt_status : %u\n", message->lidar_error_code.volt_status);
			printf("motor_status : %u\n", message->lidar_error_code.motor_status);
			printf("dirty_warn : %u\n", message->lidar_error_code.dirty_warn);
			printf("firmware_err : %u\n", message->lidar_error_code.firmware_err);
			printf("pps_status : %u\n", message->lidar_error_code.device_status);
			printf("fan_status : %u\n", message->lidar_error_code.fan_status);
			printf("self_heating : %u\n", message->lidar_error_code.self_heating);
			printf("ptp_status : %u\n", message->lidar_error_code.ptp_status);
			printf("time_sync_status : %u\n", message->lidar_error_code.time_sync_status);
			printf("system_status : %u\n", message->lidar_error_code.system_status);
		}
	}
}

double LivoxLegacyClient::getTimestamp()
{
	std::lock_guard<std::mutex> lock(m_lastTimestampMutex);
	return m_LastTimestamp;
}

/** Receiving point cloud data from Livox LiDAR. */
void LivoxLegacyClient::GetLidarData(uint8_t handle, LivoxEthPacket* data, uint32_t data_num, void* client_data)
{
	if(data)
	{
		int lidar_index = 0;
		for (int i =0; i < lidar_count; i++)
		{
			if (devices[i].handle == handle)
			{
				lidar_index = i;
				break;
			}
		}
		if (lidar_index >= lidar_count)
		{
			return;
		}


		const auto type = devices[lidar_index].info.type;
		const auto time_interval = product_type_info_pair_table[type].point_interval;


		ToUint64 toUint64;
		std::memcpy(toUint64.array, data->timestamp, sizeof(uint64_t));
		std::lock_guard<std::mutex> lcK(m_bufferLidarMutex);



		if(data->data_type == kCartesian)
		{
			data_recveive_count[handle]++;
			if(m_bufferLivoxPtr == nullptr)
			{
				return;
			}
			auto& buffer = m_bufferLivoxPtr;
			buffer->resize(buffer->size() + data_num);

			for (int i = 0; i < data_num; i++)
			{
				LivoxRawPoint* p_point_data_f = (LivoxRawPoint*)data->data;
				LivoxRawPoint* p_point_data = p_point_data_f + i;
				LivoxPoint point;
				point.x = p_point_data->x;
				point.y = p_point_data->y;
				point.z = p_point_data->z;
				point.tag = 0;
				point.reflectivity = p_point_data->reflectivity;
				point.timestamp = toUint64.data + i * time_interval;
				m_bufferLivoxPtr->push_back(point);

			}
		}
		else if(data->data_type == kSpherical)
		{
			std::cerr << "kSpherical is unsoported" << std::endl;
			LivoxSpherPoint* p_point_data = (LivoxSpherPoint*)data->data;
		}
		else if(data->data_type == kExtendCartesian)
		{
			data_recveive_count[handle]++;
			if(m_bufferLivoxPtr == nullptr)
			{
				return;
			}
			auto& buffer = m_bufferLivoxPtr;
			buffer->resize(buffer->size() + data_num);

			for (int i = 0; i < data_num; i++)
			{
				LivoxExtendRawPoint* p_point_data_f = (LivoxExtendRawPoint*)data->data;
				LivoxExtendRawPoint* p_point_data = p_point_data_f + i;
				LivoxPoint point;
				point.x = p_point_data->x;
				point.y = p_point_data->y;
				point.z = p_point_data->z;
				point.reflectivity = p_point_data->reflectivity;
				point.tag = p_point_data->tag;
				point.timestamp = toUint64.data + i * time_interval;
				m_bufferLivoxPtr->push_back(point);
			}
		}
		else if(data->data_type == kExtendSpherical)
		{
			std::cerr << "kExtendSpherical is unsoported" << std::endl;
			LivoxExtendSpherPoint* p_point_data = (LivoxExtendSpherPoint*)data->data;
		}
		else if(data->data_type == kDualExtendCartesian)
		{
			LivoxDualExtendRawPoint* p_point_data = (LivoxDualExtendRawPoint*)data->data;
		}
		else if(data->data_type == kDualExtendSpherical)
		{
			std::cerr << "kDualExtendSpherical is unsoported" << std::endl;
			LivoxDualExtendSpherPoint* p_point_data = (LivoxDualExtendSpherPoint*)data->data;
		}
		else if(data->data_type == kImu)
		{
			imu_recveive_count[handle]++;
			if(m_bufferIMUPtr == nullptr)
			{
				return;
			}
			LivoxImuPoint* p_imu_data = (LivoxImuPoint*)data->data;

			LivoxIMU point;
			point.acc_x = p_imu_data->acc_x;
			point.acc_y = p_imu_data->acc_y;
			point.acc_z = p_imu_data->acc_z;

			point.gyro_x = p_imu_data->gyro_x;
			point.gyro_y = p_imu_data->gyro_y;
			point.gyro_z = p_imu_data->gyro_z;

			point.timestamp = toUint64.data;
			m_bufferIMUPtr->push_back(point);

		}
		else if(data->data_type == kTripleExtendCartesian)
		{
			std::cerr << "kTripleExtendCartesian is unsoported" << std::endl;
			LivoxTripleExtendRawPoint* p_point_data = (LivoxTripleExtendRawPoint*)data->data;
		}
		else if(data->data_type == kTripleExtendSpherical)
		{
			std::cerr << "kTripleExtendSpherical is unsoported" << std::endl;
			LivoxTripleExtendSpherPoint* p_point_data = (LivoxTripleExtendSpherPoint*)data->data;
		}

//		if(data_recveive_count[handle] % 100 == 0)
//		{
//			/** Parsing the timestamp and the point cloud data. */
//			uint64_t cur_timestamp = *((uint64_t*)(data->timestamp));
//			if(data->data_type == kCartesian)
//			{
//				LivoxRawPoint* p_point_data = (LivoxRawPoint*)data->data;
//			}
//			else if(data->data_type == kSpherical)
//			{
//				LivoxSpherPoint* p_point_data = (LivoxSpherPoint*)data->data;
//			}
//			else if(data->data_type == kExtendCartesian)
//			{
//				LivoxExtendRawPoint* p_point_data = (LivoxExtendRawPoint*)data->data;
//			}
//			else if(data->data_type == kExtendSpherical)
//			{
//				LivoxExtendSpherPoint* p_point_data = (LivoxExtendSpherPoint*)data->data;
//			}
//			else if(data->data_type == kDualExtendCartesian)
//			{
//				LivoxDualExtendRawPoint* p_point_data = (LivoxDualExtendRawPoint*)data->data;
//			}
//			else if(data->data_type == kDualExtendSpherical)
//			{
//				LivoxDualExtendSpherPoint* p_point_data = (LivoxDualExtendSpherPoint*)data->data;
//			}
//			else if(data->data_type == kImu)
//			{
//				LivoxImuPoint* p_point_data = (LivoxImuPoint*)data->data;
//			}
//			else if(data->data_type == kTripleExtendCartesian)
//			{
//				LivoxTripleExtendRawPoint* p_point_data = (LivoxTripleExtendRawPoint*)data->data;
//			}
//			else if(data->data_type == kTripleExtendSpherical)
//			{
//				LivoxTripleExtendSpherPoint* p_point_data = (LivoxTripleExtendSpherPoint*)data->data;
//			}
//			printf("data_type %d packet num %d\n", data->data_type, data_recveive_count[handle]);
//		}
	}
}

/** Callback function of starting sampling. */
void LivoxLegacyClient::OnSampleCallback(livox_status status, uint8_t handle, uint8_t response, void* data)
{
	printf("OnSampleCallback statue %d handle %d response %d \n", status, handle, response);
	if(status == kStatusSuccess)
	{
		if(response != 0)
		{
			devices[handle].device_state = kDeviceStateConnect;
		}
	}
	else if(status == kStatusTimeout)
	{
		devices[handle].device_state = kDeviceStateConnect;
	}
}

/** Callback function of stopping sampling. */
void LivoxLegacyClient::OnStopSampleCallback(livox_status status, uint8_t handle, uint8_t response, void* data) { }

/** Query the firmware version of Livox LiDAR. */
void LivoxLegacyClient::OnDeviceInformation(livox_status status, uint8_t handle, DeviceInformationResponse* ack, void* data)
{
	if(status != kStatusSuccess)
	{
		printf("Device Query Informations Failed %d\n", status);
	}
	if(ack)
	{
		printf("firm ver: %d.%d.%d.%d\n", ack->firmware_version[0], ack->firmware_version[1], ack->firmware_version[2], ack->firmware_version[3]);
	}
}

void LivoxLegacyClient::LidarConnect(const DeviceInfo* info)
{
	uint8_t handle = info->handle;
	QueryDeviceInformation(handle, OnDeviceInformation, NULL);
	if(devices[handle].device_state == kDeviceStateDisconnect)
	{
		devices[handle].device_state = kDeviceStateConnect;
		devices[handle].info = *info;
	}
}

void LivoxLegacyClient::LidarDisConnect(const DeviceInfo* info)
{
	uint8_t handle = info->handle;
	devices[handle].device_state = kDeviceStateDisconnect;
}

void LivoxLegacyClient::LidarStateChange(const DeviceInfo* info)
{
	uint8_t handle = info->handle;
	devices[handle].info = *info;
}

/** Callback function of changing of device state. */
void LivoxLegacyClient::OnDeviceInfoChange(const DeviceInfo* info, DeviceEvent type)
{
	if(info == NULL)
	{
		return;
	}

	uint8_t handle = info->handle;
	if(handle >= kMaxLidarCount)
	{
		return;
	}
	if(type == kEventConnect)
	{
		LidarConnect(info);
		printf("[WARNING] Lidar sn: [%s] Connect!!!\n", info->broadcast_code);
	}
	else if(type == kEventDisconnect)
	{
		LidarDisConnect(info);
		printf("[WARNING] Lidar sn: [%s] Disconnect!!!\n", info->broadcast_code);
	}
	else if(type == kEventStateChange)
	{
		LidarStateChange(info);
		printf("[WARNING] Lidar sn: [%s] StateChange!!!\n", info->broadcast_code);
	}

	if(devices[handle].device_state == kDeviceStateConnect)
	{
		printf("Device Working State %d\n", devices[handle].info.state);
		if(devices[handle].info.state == kLidarStateInit)
		{
			printf("Device State Change Progress %u\n", devices[handle].info.status.progress);
		}
		else
		{
			printf("Device State Error Code 0X%08x\n", devices[handle].info.status.status_code.error_code);
		}
		printf("Device feature %d\n", devices[handle].info.feature);
		SetErrorMessageCallback(handle, OnLidarErrorStatusCallback);
		if(devices[handle].info.state == kLidarStateNormal)
		{
			LidarStartSampling(handle, OnSampleCallback, NULL);
			devices[handle].device_state = kDeviceStateSampling;
		}
	}
}

/** Callback function when broadcast message received.
 * You need to add listening device broadcast code and set the point cloud data callback in this function.
 */
void LivoxLegacyClient::OnDeviceBroadcast(const BroadcastDeviceInfo* info)
{
	if(info == NULL || info->dev_type == kDeviceTypeHub)
	{
		return;
	}

	printf("Receive Broadcast Code %s\n", info->broadcast_code);

	if(lidar_count > 0)
	{
		bool found = false;
		int i = 0;
		for(i = 0; i < lidar_count; ++i)
		{
			if(strncmp(info->broadcast_code, broadcast_code_list[i], kBroadcastCodeSize) == 0)
			{
				found = true;
				break;
			}
		}
		if(!found)
		{
			return;
		}
	}

	bool result = false;
	uint8_t handle = 0;
	result = AddLidarToConnect(info->broadcast_code, &handle);
	if(result == kStatusSuccess)
	{
		/** Set the point cloud data for a specific Livox LiDAR. */
		SetDataCallback(handle, GetLidarData, NULL);
		devices[handle].handle = handle;
		devices[handle].device_state = kDeviceStateDisconnect;
		lidar_count++;
	}
}

std::pair<mandeye::LivoxPointsBufferPtr, mandeye::LivoxIMUBufferPtr> LivoxLegacyClient::retrieveData()
{
	std::lock_guard<std::mutex> lck1(m_bufferLidarMutex);
	std::lock_guard<std::mutex> lck2(m_bufferImuMutex);
	LivoxPointsBufferPtr returnPointerLidar{std::make_shared<LivoxPointsBuffer>()};
	LivoxIMUBufferPtr returnPointerImu{std::make_shared<LivoxIMUBuffer>()};
	std::swap(m_bufferIMUPtr, returnPointerImu);
	std::swap(m_bufferLivoxPtr, returnPointerLidar);
	return std::pair<LivoxPointsBufferPtr, LivoxIMUBufferPtr>(returnPointerLidar, returnPointerImu);
}

void LivoxLegacyClient::startLog()
{
	std::lock_guard<std::mutex> lcK1(m_bufferLidarMutex);
	std::lock_guard<std::mutex> lcK2(m_bufferImuMutex);
	m_bufferLivoxPtr = std::make_shared<LivoxPointsBuffer>();
	m_bufferIMUPtr = std::make_shared<LivoxIMUBuffer>();
}

void LivoxLegacyClient::stopLog()
{
	std::lock_guard<std::mutex> lcK1(m_bufferLidarMutex);
	std::lock_guard<std::mutex> lcK2(m_bufferImuMutex);
	m_bufferLivoxPtr = nullptr;
	m_bufferIMUPtr = nullptr;
}
} // namespace mandeye