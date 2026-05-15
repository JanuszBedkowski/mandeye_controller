#pragma once

#include "lidars/BaseLidarClient.h"
#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace mandeye
{

class ReplayLidar : public BaseLidarClient
{
public:
	ReplayLidar() = default;
	~ReplayLidar() override;

	void Init(const nlohmann::json& config) override;
	nlohmann::json produceStatus() override;

	//! path is used as the replay directory if replay_path was not set via Init()
	bool startListener(const std::string& path) override;
	void stopListener() override;

	void startLog() override;
	void stopLog() override;

	std::pair<LidarPointsBufferPtr, LidarIMUBufferPtr> retrieveData() override;

	double getTimestamp() override;
	double getSessionDuration() override;
	double getSessionStart() override;
	void initializeDuration() override;
	bool isSynced() override
	{
		return true;
	}

	// LAZ/CSV → MCAP conversion. Returns path to the written file, or empty on failure.
	static std::filesystem::path convertToMcap(const std::filesystem::path& dir);

private:
	void DataThreadFunction();

	// LAZ/CSV loaders (kept for conversion step)
	static LidarPointsBufferPtr loadLaz(const std::filesystem::path& path);
	static LidarIMUBufferPtr loadCsv(const std::filesystem::path& path);

	std::string m_replayPath;
	std::filesystem::path m_mcapPath;

	std::mutex m_bufferMutex;
	LidarPointsBufferPtr m_bufferLidarPtr;
	LidarIMUBufferPtr m_bufferIMUPtr;

	std::thread m_workerThread;
	std::atomic_bool m_isDone{false};

	std::atomic_int m_currentChunk{0};
	std::atomic_int m_totalChunks{0};
	std::atomic<double> m_currentTimestamp{0.0};
	double m_sessionStart{0.0};
};

} // namespace mandeye
