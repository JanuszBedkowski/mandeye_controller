#include "lidars/replay/ReplayLidar.h"
#include "../../mcap/McapWriter.h"
#include "../../mcap/cdr_serializer.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <laszip/laszip_api.h>
#include <mcap/reader.hpp>
#include <sstream>
#include <thread>
#include <vector>

namespace mandeye
{

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

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
	data["ReplayLidar"]["mcap_path"] = m_mcapPath.string();
	data["ReplayLidar"]["current_chunk"] = m_currentChunk.load();
	data["ReplayLidar"]["total_chunks"] = m_totalChunks.load();
	data["ReplayLidar"]["current_timestamp"] = m_currentTimestamp.load();
	return data;
}

bool ReplayLidar::startListener(const std::string& path)
{
	if(m_replayPath.empty())
		m_replayPath = path;

	namespace fs = std::filesystem;
	const fs::path dir(m_replayPath);
	const fs::path mcap = dir / "session.mcap";

	if(fs::exists(mcap))
	{
		std::cout << "ReplayLidar: session.mcap found, skipping conversion\n";
	}
	else
	{
		std::cout << "ReplayLidar: converting laz/csv → session.mcap …\n";
		const fs::path result = convertToMcap(dir);
		if(result.empty())
		{
			std::cerr << "ReplayLidar: conversion failed — no laz files in " << dir << "\n";
			return false;
		}
		std::cout << "ReplayLidar: conversion done → " << result << "\n";
	}

	m_mcapPath = mcap;
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
// LAZ / CSV loaders (used only during conversion)
// ---------------------------------------------------------------------------

LidarPointsBufferPtr ReplayLidar::loadLaz(const std::filesystem::path& path)
{
	auto buffer = std::make_shared<LidarPointsBuffer>();

	laszip_POINTER reader;
	if(laszip_create(&reader))
	{
		std::cerr << "ReplayLidar: laszip_create failed for " << path << "\n";
		return buffer;
	}

	laszip_BOOL is_compressed = 0;
	if(laszip_open_reader(reader, path.c_str(), &is_compressed))
	{
		std::cerr << "ReplayLidar: cannot open " << path << "\n";
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
	std::getline(file, line); // skip header

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

// ---------------------------------------------------------------------------
// LAZ/CSV → MCAP conversion
// ---------------------------------------------------------------------------

std::filesystem::path ReplayLidar::convertToMcap(const std::filesystem::path& dir)
{
	namespace fs = std::filesystem;

	std::vector<int> chunks;
	try
	{
		for(const auto& entry : fs::directory_iterator(dir))
		{
			int idx = -1;
			if(std::sscanf(entry.path().filename().string().c_str(), "lidar%d.laz", &idx) == 1)
				chunks.push_back(idx);
		}
	}
	catch(const std::exception& e)
	{
		std::cerr << "ReplayLidar: directory error: " << e.what() << "\n";
		return {};
	}

	if(chunks.empty())
		return {};
	std::sort(chunks.begin(), chunks.end());

	const fs::path mcapPath = dir / "session.mcap";
	McapFileWriter writer(mcapPath, "lidar");
	if(!writer.isOpen())
		return {};

	static constexpr uint64_t kWindowNs = 10'000'000ULL; // 10 ms

	for(int idx : chunks)
	{
		char lazName[64], csvName[64];
		std::snprintf(lazName, sizeof(lazName), "lidar%04d.laz", idx);
		std::snprintf(csvName, sizeof(csvName), "imu%04d.csv", idx);

		auto pts = loadLaz(dir / lazName);
		auto imu = loadCsv(dir / csvName);

		std::multimap<uint64_t, std::variant<LidarPointsBuffer, LidarIMU>> dataSorted;

		if(!pts->empty())
		{
			std::sort(pts->begin(), pts->end(), [](const LidarPoint& a, const LidarPoint& b) { return a.timestamp < b.timestamp; });

			uint64_t windowEnd = (pts->front().timestamp / kWindowNs + 1) * kWindowNs;
			LidarPointsBuffer window;

			for(const auto& p : *pts)
			{
				if(p.timestamp >= windowEnd)
				{
					dataSorted.emplace(windowEnd, std::move(window));

					window = {};
					windowEnd = (p.timestamp / kWindowNs + 1) * kWindowNs;
				}
				window.push_back(p);
			}
			if(!window.empty())
				dataSorted.emplace(windowEnd, std::move(window));
		}
		// add imus
		for(const auto& imu_msg : *imu)
		{
			dataSorted.emplace(imu_msg.timestamp, std::move(imu_msg));
		}

		// save to mcap
		for(const auto& [ts, data] : dataSorted)
		{
			if(std::holds_alternative<LidarPointsBuffer>(data))
			{
				const auto& w = std::get<LidarPointsBuffer>(data);
				writer.writePointCloud(w.back().timestamp, w);
			}
			else
				writer.writeImuSample(std::get<LidarIMU>(data));
		}
		std::cout << "  chunk " << idx << " → " << pts->size() << " pts, " << imu->size() << " imu\n";
	}

	return mcapPath;
}

// ---------------------------------------------------------------------------
// Replay thread — reads session.mcap and loops forever
// ---------------------------------------------------------------------------

void ReplayLidar::DataThreadFunction()
{
	while(!m_isDone)
	{
		mcap::McapReader reader;
		auto status = reader.open(m_mcapPath.string());
		if(!status.ok())
		{
			std::cerr << "ReplayLidar: cannot open " << m_mcapPath << ": " << status.message << "\n";
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}

		auto view = reader.readMessages();

		bool started = false;
		uint64_t mcapStartNs = 0;
		auto wallStart = std::chrono::steady_clock::now();
		int msgIdx = 0;

		for(auto it = view.begin(); !m_isDone && it != view.end(); ++it)
		{
			const auto& mv = *it;
			const uint64_t msgNs = mv.message.logTime;

			if(!started)
			{
				mcapStartNs = msgNs;
				wallStart = std::chrono::steady_clock::now();
				m_sessionStart = msgNs * 1e-9;
				started = true;
			}

			// Real-time pacing: sleep until this message's wall-clock due time.

			m_currentTimestamp = msgNs * 1e-9;
			m_currentChunk = ++msgIdx;

			const auto mcapDurationNs = msgNs - mcapStartNs;
			const auto wallDue = wallStart + std::chrono::nanoseconds(msgNs - mcapStartNs);
			std::this_thread::sleep_until(wallDue);

			const auto* bytes = reinterpret_cast<const uint8_t*>(mv.message.data);
			const size_t sz = mv.message.dataSize;
			const std::string& topic = mv.channel->topic;

			if(topic == "/lidar_points")
			{
				auto pts = decodePc2(bytes, sz);
				if(pts && !pts->empty())
				{
					std::lock_guard<std::mutex> lock(m_bufferMutex);
					if(m_bufferLidarPtr)
						m_bufferLidarPtr->insert(m_bufferLidarPtr->end(), pts->begin(), pts->end());
				}
			}
			else if(topic == "/imu")
			{
				auto imu = decodeImu(bytes, sz);
				if(imu)
				{
					std::lock_guard<std::mutex> lock(m_bufferMutex);
					if(m_bufferIMUPtr)
						m_bufferIMUPtr->push_back(*imu);
				}
			}

		}

		reader.close();

		// Brief pause before looping back (avoids spin if file is very short)
		if(!m_isDone)
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
}

} // namespace mandeye
