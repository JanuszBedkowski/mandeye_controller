#include "publisher.h"

namespace mandeye
{
Publisher::Publisher()
	: m_context(1)
	, m_publisher(m_context, ZMQ_PUB)
{
	m_publisher.bind("tcp://*:5556");
	m_publisher.setsockopt(ZMQ_CONFLATE, 1); // Keep only the latest task
	m_thread = std::thread(&Publisher::worker, this);
}
Publisher::~Publisher()
{
	m_running = false;
	m_thread.join();
	m_publisher.close();
}

void Publisher::publish(const nlohmann::json& data)
{
	auto dataMessage = data.dump();
	zmq::message_t message(dataMessage.c_str(), dataMessage.size());
	m_publisher.send(message, zmq::send_flags::none);
}

void Publisher::worker()
{
	while(m_running)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(25));
		const double time = GetTimeStamp();
		const double elapsed = GetSessionDuration();
		nlohmann::json data;
		if(std::floor(time) != std::floor(m_lastTime))
		{
			// longer report
			std::unique_lock<std::mutex> lock(m_mutex);
			data["time"] = static_cast<uint64_t>(time*1e9);
			data["dur"] = static_cast<uint64_t>(elapsed*1e9);
			data["stopScanDirectory"] = m_stopScanDirectory;
			data["continousScanDirectory"] = m_continousScanDirectory;
			data["mode"] = m_mode;
			m_lastTime = time;
		}
		else
		{
			// short report
			std::unique_lock<std::mutex> lock(m_mutex);
			data["time"] = static_cast<uint64_t>(time*1e9);
			data["dur"] = static_cast<uint64_t>(elapsed*1e9);
		}
		publish(data);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

	}
}

void Publisher::SetWorkingDirectory(const std::string& stopScanDirectory, const std::string& continousScanDirectory)
{
	std::unique_lock<std::mutex> lock(m_mutex);
	m_stopScanDirectory = stopScanDirectory;
	m_continousScanDirectory = continousScanDirectory;
}
void Publisher::SetMode(const std::string& mode)
{
	std::unique_lock<std::mutex> lock(m_mutex);
	m_mode = mode;
}

} // namespace mandeye
