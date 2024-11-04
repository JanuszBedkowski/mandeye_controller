#pragma once
#include <json.hpp>
#include <zmq.hpp>
#include "utils/TimeStampReceiver.h"
#include <thread>
#include <atomic>
#include <mutex>
namespace mandeye
{
class Publisher : public mandeye_utils::TimeStampReceiver
{
public:
	std::string_view ContinuesMode = "ContinuesMode";
	std::string_view StopScanMode = "ChunkedMode";
	Publisher();
	~Publisher();
	Publisher(const Publisher&) = delete;
	Publisher& operator=(const Publisher&) = delete;
	void publish(const nlohmann::json& data);
	void SetWorkingDirectory(const std::string& stopScanDirectory, const std::string& continousScanDirectory);
	void SetMode(const std::string& mode);
private:
	void worker();
	std::atomic<bool> m_running{true};
	std::string m_continousScanDirectory;
	std::string m_stopScanDirectory;
	std::string m_mode;
	zmq::context_t m_context;
	zmq::socket_t m_publisher;
	std::thread m_thread;
	std::mutex m_mutex;
	double m_lastTime=0;
};
}