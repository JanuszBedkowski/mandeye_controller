#include "lidars/hesai/HesaiClient.h"

#include <chrono>
#include <iostream>
namespace mandeye
{

nlohmann::json HesaiClient::produceStatus()
{
	nlohmann::json data;

	nlohmann::json data_status;
	data_status["init_success"] = true;
	data["is_synced"] = isSynced();
	data_status["is_done"] = isDone.load(); // Current status of the data thread
	data_status["received_point_messages"] = m_recivedPointMessages.load(); // Number of received point messages
	data_status["received_imu_messages"] = m_recivedIMUMessages.load(); // Number of received IMU messages
	data_status["lidar_state"] = m_lidar_state;
	data_status["work_mode"] = m_work_mode;
	data_status["packet_num"] = m_packet_num;
	data_status["software_vers"] = m_software_vers;
	data_status["hardware_vers"] = m_hardware_vers;
	data_status["laser_num"] = m_laser_num;
	data_status["channel_num"] = m_channel_num;
	data_status["timestamp_sec"] = m_timestamp;
	data_status["time_diff"] = m_time_diff;
	nlohmann::json faults;
	for(auto& fault : m_faults)
	{
		nlohmann::json fault_info;
		fault_info["fault_state"] = fault.fault_state;
		fault_info["fault_parse_version"] = fault.fault_parse_version;
		fault_info["faultcode"] = fault.faultcode;
		fault_info["faultcode_id"] = fault.faultcode_id;
		fault_info["timestamp"] = fault.timestamp;
		fault_info["timestamp_sec"] = fault.timestamp_sec;
		fault_info["operate_state"] = fault.operate_state;
		fault_info["total_faultcode_num"] = fault.total_faultcode_num;
		faults.push_back(fault_info);
	}
	data["HesaiClient"]["faults"] = faults;
	data["HesaiClient"]["status"] = data_status;
	data["counters"]["imu"] = m_recivedIMUMessages.load();
	data["counters"]["lidar"] = m_recivedPointMessages.load();

	return data;
}
bool HesaiClient::startListener(const std::string& interfaceIp)
{
	std::cout << "HesaiClient: startListener called with interfaceIp: " << interfaceIp << std::endl;
	std::lock_guard<std::mutex> lock(m_bufferImuMutex);
	// Simulate starting the listener
	m_watchThread = std::thread(&HesaiClient::DataThreadFunction, this);
	return true; // Simulate success
}

void HesaiClient::startLog()
{
	std::cout << "HesaiClient: startLog called" << std::endl;
	// Initialize buffers
	std::lock_guard<std::mutex> lock1(m_bufferImuMutex);
	std::lock_guard<std::mutex> lock2(m_bufferPointMutex);

	m_bufferLidarPtr = std::make_shared<LidarPointsBuffer>();
	m_bufferIMUPtr = std::make_shared<LidarIMUBuffer>();
	// Simulate starting log
	std::cout << "HesaiClient: Logging started" << std::endl;
}

void HesaiClient::stopLog()
{
	std::lock_guard<std::mutex> lock1(m_bufferImuMutex);
	std::lock_guard<std::mutex> lock2(m_bufferPointMutex);

	// Stop the data thread
	m_bufferLidarPtr.reset();
	m_bufferIMUPtr.reset();
	std::cout << "HesaiClient: stopLog called" << std::endl;
}

std::pair<LidarPointsBufferPtr, LidarIMUBufferPtr> HesaiClient::retrieveData()
{
	std::cout << "HesaiClient: retrieveData called" << std::endl;
	// Return empty buffers for now

	std::lock_guard<std::mutex> lock1(m_bufferImuMutex);
	std::lock_guard<std::mutex> lock2(m_bufferPointMutex);

	LidarPointsBufferPtr returnPointerLidar{std::make_shared<LidarPointsBuffer>()};
	LidarIMUBufferPtr returnPointerImu{std::make_shared<LidarIMUBuffer>()};
	std::swap(m_bufferIMUPtr, returnPointerImu);
	std::swap(m_bufferLidarPtr, returnPointerLidar);
	return std::pair<LidarPointsBufferPtr, LidarIMUBufferPtr>(returnPointerLidar, returnPointerImu);
}

void HesaiClient::DataThreadFunction()
{

	std::cout << "HesaiClient: DataThreadFunction started" << std::endl;
	DriverParam param;
	param.input_param.source_type = DATA_FROM_LIDAR;
	param.input_param.device_ip_address = "192.168.1.201"; // lidar ip
	param.input_param.ptc_port = 9347; // lidar ptc port
	param.input_param.udp_port = 2368; // point cloud destination port
	param.input_param.device_fault_port = 2369; // lidar fault port
	param.input_param.multicast_ip_address = "";
	param.input_param.ptc_mode = PtcMode::tcp;
	param.input_param.use_ptc_connected = true; // true: use PTC connected, false: recv correction from local file
	param.input_param.correction_file_path = "";
	param.input_param.firetimes_path = "";

	param.input_param.host_ip_address = ""; // point cloud destination ip, local ip
	param.input_param.fault_message_port = 0; // fault message destination port, 0: not use
	// PtcMode::tcp_ssl use
	param.input_param.certFile = "";
	param.input_param.privateKeyFile = "";
	param.input_param.caFile = "";

	param.decoder_param.enable_packet_loss_tool = false;
	param.decoder_param.socket_buffer_size = 262144000;
	//init lidar with param

	m_lidar = std::make_unique<HesaiLidarSdk<LidarPointXYZICRT>>();
	m_lidar->Init(param);
	m_lidar->RegRecvCallback([this](const LidarDecodedFrame<LidarPointXYZICRT>& dataFrame) { CallbackFrame(dataFrame); });

	m_lidar->RegRecvCallback([this](const LidarImuData& dataFrame) { CallbackIMU(dataFrame); });

	m_lidar->RegRecvCallback([this](const FaultMessageInfo& fault_message_info) { CallbackFault(fault_message_info); });
	m_lidar->Start();

	while(!isDone)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
	m_lidar->Stop();
	std::cout << "HesaiClient: DataThreadFunction ended" << std::endl;
}
void HesaiClient::CallbackFrame(const LidarDecodedFrame<LidarPointXYZICRT>& dataFrame)
{

	m_recivedPointMessages.fetch_add(1);
	m_lidar_state = dataFrame.lidar_state;
	m_work_mode = dataFrame.work_mode;
	m_packet_num = dataFrame.packet_num;
	m_software_vers = dataFrame.software_version;
	m_hardware_vers = dataFrame.hardware_version;
	m_laser_num = dataFrame.laser_num;
	m_channel_num = dataFrame.channel_num;
	if(dataFrame.points_num > 0)
	{
		m_timestamp = dataFrame.points[0].timestamp;
		//get current timestamp
		using namespace std::chrono;
		const auto now = system_clock::now();
		auto duration = now.time_since_epoch();
		double tp = std::chrono::duration<double>(duration).count();
		m_time_diff = std::abs(tp - m_timestamp);
	}

	std::lock_guard<std::mutex> lock(m_bufferPointMutex);
	if(m_bufferLidarPtr)
	{
		for(size_t i = 0; i < dataFrame.points_num; ++i)
		{
			const auto& point = dataFrame.points[i];
			LidarPoint data;
			data.x = point.x;
			data.y = point.y;
			data.z = point.z;
			data.intensity = point.intensity;
			data.laser_id = 0;
			data.timestamp = point.timestamp;
			m_timestamp = point.timestamp;
			m_bufferLidarPtr->push_back(data);
		}
	}
}

void HesaiClient::CallbackIMU(const LidarImuData& dataFrame)
{
	m_recivedIMUMessages.fetch_add(1);
	std::lock_guard<std::mutex> lock(m_bufferImuMutex);
	if(m_bufferIMUPtr)
	{
		auto now = std::chrono::system_clock::now();
		auto duration = now.time_since_epoch();
		auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

		LidarIMU data;
		data.timestamp = dataFrame.timestamp;
		data.acc_x = dataFrame.imu_accel_x;
		data.acc_y = dataFrame.imu_accel_y;
		data.acc_z = dataFrame.imu_accel_z;
		data.gyro_x = dataFrame.imu_ang_vel_x;
		data.gyro_y = dataFrame.imu_ang_vel_y;
		data.gyro_z = dataFrame.imu_ang_vel_z;
		data.laser_id = 0;
		data.epoch_time = millis.count();
		m_bufferIMUPtr->push_back(data);
	}
}

void HesaiClient::CallbackFault(const FaultMessageInfo& fault_message_info)
{
	std::cerr << "HesaiClient: CallbackFault called with fault message: " << fault_message_info.fault_state << std::endl;
	m_faults.push_back(fault_message_info);
	if(m_faults.size() > 10)
	{
		m_faults.pop_front();
	}
}

} // namespace mandeye
