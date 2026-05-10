#include "lidars/replay/ReplayLidar.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <laszip/laszip_api.h>
#include <sstream>
#include <thread>
#include <vector>

namespace mandeye
{

ReplayLidar::~ReplayLidar()
{
	m_isDone = true;
	if(m_workerThread.joinable())
		m_workerThread.join();
}

void ReplayLidar::Init(const nlohmann::json& config)
{
	if(config.contains("replay_path"))
		m_replayPath = config["replay_path"].get<std::string>();
}

nlohmann::json ReplayLidar::produceStatus()
{
	nlohmann::json data;
	data["ReplayLidar"]["replay_path"] = m_replayPath;
	data["ReplayLidar"]["current_chunk"] = m_currentChunk.load();
	data["ReplayLidar"]["total_chunks"] = m_totalChunks.load();
	data["ReplayLidar"]["current_timestamp"] = m_currentTimestamp.load();
	return data;
}

bool ReplayLidar::startListener(const std::string& path)
{
	if(m_replayPath.empty())
		m_replayPath = path;

	std::cout << "ReplayLidar: starting replay from " << m_replayPath << std::endl;
	m_workerThread = std::thread(&ReplayLidar::DataThreadFunction, this);
	return true;
}

void ReplayLidar::stopListener()
{
	m_isDone = true;
	if(m_workerThread.joinable())
		m_workerThread.join();
}

void ReplayLidar::startLog()
{
	std::lock_guard<std::mutex> lock(m_bufferMutex);
	m_bufferLidarPtr = std::make_shared<LidarPointsBuffer>();
	m_bufferIMUPtr = std::make_shared<LidarIMUBuffer>();
}

void ReplayLidar::stopLog()
{
	std::lock_guard<std::mutex> lock(m_bufferMutex);
	m_bufferLidarPtr.reset();
	m_bufferIMUPtr.reset();
}

std::pair<LidarPointsBufferPtr, LidarIMUBufferPtr> ReplayLidar::retrieveData()
{
	std::lock_guard<std::mutex> lock(m_bufferMutex);

	LidarPointsBufferPtr retLidar = std::make_shared<LidarPointsBuffer>();
	LidarIMUBufferPtr retImu = std::make_shared<LidarIMUBuffer>();
	std::swap(m_bufferLidarPtr, retLidar);
	std::swap(m_bufferIMUPtr, retImu);
	return {retLidar, retImu};
}

double ReplayLidar::getTimestamp()
{
	return m_currentTimestamp.load();
}

double ReplayLidar::getSessionDuration()
{
	return m_currentTimestamp.load() - m_sessionStart;
}

double ReplayLidar::getSessionStart()
{
	return m_sessionStart;
}

void ReplayLidar::initializeDuration()
{
	m_sessionStart = m_currentTimestamp.load();
}

// ---------------------------------------------------------------------------

LidarPointsBufferPtr ReplayLidar::loadLaz(const std::filesystem::path& path)
{
	auto buffer = std::make_shared<LidarPointsBuffer>();

	laszip_POINTER reader;
	if(laszip_create(&reader))
	{
		std::cerr << "ReplayLidar: laszip_create failed for " << path << std::endl;
		return buffer;
	}

	laszip_BOOL is_compressed = 0;
	if(laszip_open_reader(reader, path.c_str(), &is_compressed))
	{
		std::cerr << "ReplayLidar: cannot open " << path << std::endl;
		laszip_destroy(reader);
		return buffer;
	}

	laszip_header* header = nullptr;
	laszip_get_header_pointer(reader, &header);

	laszip_point* point = nullptr;
	laszip_get_point_pointer(reader, &point);

	const laszip_I64 num_points = header->number_of_point_records;
	laszip_F64 coords[3];

	for(laszip_I64 i = 0; i < num_points; ++i)
	{
		if(laszip_read_point(reader))
			break;
		if(laszip_get_coordinates(reader, coords))
			break;

		LidarPoint p{};
		p.x = static_cast<float>(coords[0]);
		p.y = static_cast<float>(coords[1]);
		p.z = static_cast<float>(coords[2]);
		p.intensity = static_cast<float>(point->intensity);
		p.timestamp = static_cast<uint64_t>(point->gps_time * 1e9);
		p.tag = point->classification;
		p.laser_id = point->user_data;
		buffer->push_back(p);
	}

	laszip_close_reader(reader);
	laszip_destroy(reader);
	return buffer;
}

LidarIMUBufferPtr ReplayLidar::loadCsv(const std::filesystem::path& path)
{
	auto buffer = std::make_shared<LidarIMUBuffer>();

	std::ifstream file(path);
	if(!file.is_open())
		return buffer;

	std::string line;
	std::getline(file, line); // skip header: timestamp gyroX gyroY gyroZ accX accY accZ imuId timestampUnix

	while(std::getline(file, line))
	{
		if(line.empty())
			continue;
		std::istringstream ss(line);
		LidarIMU imu{};
		ss >> imu.timestamp >> imu.gyro_x >> imu.gyro_y >> imu.gyro_z >> imu.acc_x >> imu.acc_y >> imu.acc_z >> imu.laser_id >> imu.epoch_time;
		if(!ss.fail())
			buffer->push_back(imu);
	}
	return buffer;
}

void ReplayLidar::DataThreadFunction()
{
	namespace fs = std::filesystem;

	while(!m_isDone)
	{
		// collect sorted chunk indices from lidar*.laz files
		std::vector<int> chunks;
		try
		{
			for(const auto& entry : fs::directory_iterator(m_replayPath))
			{
				const std::string name = entry.path().filename().string();
				int idx = -1;
				if(std::sscanf(name.c_str(), "lidar%d.laz", &idx) == 1)
					chunks.push_back(idx);
			}
		}
		catch(const std::exception& e)
		{
			std::cerr << "ReplayLidar: directory error: " << e.what() << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(2));
			continue;
		}

		if(chunks.empty())
		{
			std::cerr << "ReplayLidar: no lidar*.laz files in " << m_replayPath << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(2));
			continue;
		}

		std::sort(chunks.begin(), chunks.end());
		m_totalChunks = static_cast<int>(chunks.size());

		for(int idx : chunks)
		{
			if(m_isDone)
				break;

			char lazName[64], csvName[64];
			std::snprintf(lazName, sizeof(lazName), "lidar%04d.laz", idx);
			std::snprintf(csvName, sizeof(csvName), "imu%04d.csv", idx);

			const fs::path lazPath = fs::path(m_replayPath) / lazName;
			const fs::path csvPath = fs::path(m_replayPath) / csvName;

			std::cout << "ReplayLidar: loading chunk " << idx << " from " << lazPath << std::endl;

			auto lidarChunk = loadLaz(lazPath);
			auto imuChunk = loadCsv(csvPath);

			m_currentChunk = idx;

			if(lidarChunk->empty())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			// derive chunk duration from first/last point timestamps
			const uint64_t tsFirst = lidarChunk->front().timestamp;
			const uint64_t tsLast = lidarChunk->back().timestamp;
			const auto chunkDuration = std::chrono::nanoseconds(tsLast > tsFirst ? tsLast - tsFirst : 0);

			m_currentTimestamp = lidarChunk->back().timestamp * 1e-9;
			if(m_sessionStart == 0.0)
				m_sessionStart = lidarChunk->front().timestamp * 1e-9;

			{
				std::lock_guard<std::mutex> lock(m_bufferMutex);
				if(m_bufferLidarPtr)
					m_bufferLidarPtr->insert(m_bufferLidarPtr->end(), lidarChunk->begin(), lidarChunk->end());
				if(m_bufferIMUPtr)
					m_bufferIMUPtr->insert(m_bufferIMUPtr->end(), imuChunk->begin(), imuChunk->end());
			}

			// sleep for the real duration the chunk covers (minimum 100 ms)
			const auto sleepDuration = chunkDuration.count() > 0 ? chunkDuration : std::chrono::milliseconds(100);
			const auto deadline = std::chrono::steady_clock::now() + sleepDuration;
			while(!m_isDone && std::chrono::steady_clock::now() < deadline)
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
	}
}

} // namespace mandeye