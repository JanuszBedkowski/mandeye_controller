#include "publisher.h"

namespace mandeye
{
Publisher::Publisher()
	: m_context(1)
	, m_publisher(m_context, ZMQ_PUB)
{
	m_publisher.bind("tcp://*:5556");
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
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		const double time = GetTimeStamp();
		if(std::floor(time) != std::floor(m_lastTime))
		{
			nlohmann::json data;
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				data["time"] = time;
				data["stopScanDirectory"] = m_stopScanDirectory;
				data["continousScanDirectory"] = m_continousScanDirectory;
				data["mode"] = m_mode;
			}
			publish(data);
		}
		m_lastTime = time;
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