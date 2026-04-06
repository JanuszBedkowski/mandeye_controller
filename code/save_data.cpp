#include "save_data.h"
#include "hardware_config/mandeye.h"
#include "save_laz.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace mandeye
{

std::pair<std::string, std::optional<LazStats>> savePointcloudData(LidarPointsBufferPtr buffer, const std::string& directory, int chunk)
{
	using namespace std::chrono_literals;
	char lidarName[256];

	const auto start = std::chrono::steady_clock::now();
	snprintf(lidarName, 256, "lidar%04d.laz", chunk);
	std::filesystem::path lidarFilePath = std::filesystem::path(directory) / std::filesystem::path(lidarName);
	std::cout << "Savig lidar buffer of size " << buffer->size() << " to " << lidarFilePath << std::endl;
	auto saveStatus = saveLaz(lidarFilePath.string(), buffer);

	system("sync");
	const auto end = std::chrono::steady_clock::now();
	const std::chrono::duration<float> elapsed_seconds = end - start;
	if(saveStatus)
	{
		saveStatus->m_saveDurationSec2 = elapsed_seconds.count();
		hardware::OnSavedLaz(lidarFilePath);
	}
	else
	{
		std::cout << "Error saving laz file " << lidarFilePath << std::endl;
	}
	return {lidarFilePath.string(), saveStatus};
}

void saveLidarList(const std::unordered_map<uint32_t, std::string>& lidars, const std::string& directory, int chunk)
{
	using namespace std::chrono_literals;
	char lidarName[256];
	snprintf(lidarName, 256, "lidar%04d.sn", chunk);
	std::filesystem::path lidarFilePath = std::filesystem::path(directory) / std::filesystem::path(lidarName);
	std::cout << "Savig lidar list of size " << lidars.size() << " to " << lidarFilePath << std::endl;

	std::ofstream lidarStream(lidarFilePath);
	for(const auto& [id, sn] : lidars)
	{
		lidarStream << id << " " << sn << "\n";
	}
	system("sync");
	return;
}

void saveImuData(LidarIMUBufferPtr buffer, const std::string& directory, int chunk)
{
	using namespace std::chrono_literals;
	char lidarName[256];
	snprintf(lidarName, 256, "imu%04d.csv", chunk);
	std::filesystem::path lidarFilePath = std::filesystem::path(directory) / std::filesystem::path(lidarName);
	std::cout << "Savig imu buffer of size " << buffer->size() << " to " << lidarFilePath << std::endl;
	std::ofstream lidarStream(lidarFilePath.c_str());
	lidarStream << "timestamp gyroX gyroY gyroZ accX accY accZ imuId timestampUnix\n";
	std::stringstream ss;

	for(const auto& p : *buffer)
	{
		if(p.timestamp > 0)
		{
			ss << p.timestamp << " " << p.gyro_x << " " << p.gyro_y << " " << p.gyro_z << " " << p.acc_x << " " << p.acc_y << " " << p.acc_z << " "
			   << p.laser_id << " " << p.epoch_time << "\n";
		}
	}
	lidarStream << ss.rdbuf();

	lidarStream.close();
	system("sync");
	return;
}

void saveGnssData(std::deque<std::string>& buffer, const std::string& directory, int chunk)
{
	using namespace std::chrono_literals;
	char lidarName[256];
	snprintf(lidarName, 256, "gnss%04d.gnss", chunk);
	std::filesystem::path lidarFilePath = std::filesystem::path(directory) / std::filesystem::path(lidarName);
	std::cout << "Savig gnss buffer of size " << buffer.size() << " to " << lidarFilePath << std::endl;
	std::ofstream lidarStream(lidarFilePath.c_str());
	std::stringstream ss;

	for(const auto& p : buffer)
	{
		ss << p;
	}
	lidarStream << ss.rdbuf();

	lidarStream.close();
	system("sync");
	return;
}

void saveGnssRawData(std::deque<std::string>& buffer, const std::string& directory, int chunk)
{
	using namespace std::chrono_literals;
	char lidarName[256];
	snprintf(lidarName, 256, "gnss%04d.nmea", chunk);
	std::filesystem::path lidarFilePath = std::filesystem::path(directory) / std::filesystem::path(lidarName);
	std::cout << "Savig gnss raw buffer of size " << buffer.size() << " to " << lidarFilePath << std::endl;
	std::ofstream lidarStream(lidarFilePath.c_str());
	std::stringstream ss;

	for(const auto& p : buffer)
	{
		ss << p;
	}
	lidarStream << ss.rdbuf();

	lidarStream.close();
	system("sync");
	return;
}

} // namespace mandeye