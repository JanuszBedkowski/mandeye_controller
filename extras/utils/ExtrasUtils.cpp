#include "ExtrasUtils.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <zmq.hpp>

namespace mandeye::extras
{

std::string getEnvString(const std::string& env, const std::string& def)
{
	const char* env_p = std::getenv(env.c_str());
	if(env_p == nullptr)
		return def;
	return std::string{env_p};
}

void startZeroMQListener(const StatusCallback& callback)
{
	try
	{
		zmq::context_t context(1);
		zmq::socket_t socket(context, zmq::socket_type::sub);
		socket.connect(keys::ZMQ_ENDPOINT);
		socket.set(zmq::sockopt::subscribe, "");
		socket.set(zmq::sockopt::conflate, 1);

		std::cout << "Connected to " << keys::ZMQ_ENDPOINT << std::endl;
		std::cout << "Waiting for messages..." << std::endl;

		while(true)
		{
			zmq::message_t message;
			auto result = socket.recv(message, zmq::recv_flags::none);
			if(result)
			{
				const std::string msg_str(static_cast<char*>(message.data()), message.size());
				const nlohmann::json j = nlohmann::json::parse(msg_str);
				if(j.is_object())
				{
					callback(j);
				}
			}
		}
	}
	catch(const zmq::error_t& e)
	{
		std::cerr << "ZeroMQ Error: " << e.what() << std::endl;
		std::abort();
	}
}

StatusReporter::StatusReporter(std::string serviceName, std::chrono::milliseconds heartbeat, std::chrono::milliseconds timeout)
	: m_serviceName(std::move(serviceName))
	, m_heartbeat(heartbeat)
	, m_timeout(timeout)
{
	m_thread = std::thread(&StatusReporter::worker, this);
}

StatusReporter::~StatusReporter()
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_running = false;
	}
	m_cv.notify_all();
	if(m_thread.joinable())
	{
		m_thread.join();
	}
}

void StatusReporter::reportOk(const std::string& message)
{
	report(true, message);
}

void StatusReporter::reportFailure(const std::string& message)
{
	report(false, message);
}

void StatusReporter::report(bool ok, const std::string& message, const nlohmann::json& data)
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if(m_ok == ok && m_message == message && m_data == data)
		{
			return;
		}
		m_ok = ok;
		m_message = message;
		m_data = data;
		m_dirty = true;
	}
	m_cv.notify_all();
}

void StatusReporter::worker()
{
	try
	{
		// the socket lives on this thread only (ZMQ sockets are not thread-safe)
		zmq::context_t context(1);
		zmq::socket_t socket(context, zmq::socket_type::push);
		socket.set(zmq::sockopt::linger, 0);
		socket.set(zmq::sockopt::conflate, 1); // controller down: keep only the latest state
		socket.connect(keys::ZMQ_STATUS_ENDPOINT);

		std::unique_lock<std::mutex> lock(m_mutex);
		while(m_running)
		{
			nlohmann::json j;
			j["service"] = m_serviceName;
			j["ok"] = m_ok;
			j["message"] = m_message;
			j["timeout_s"] = std::chrono::duration<double>(m_timeout).count();
			j["data"] = m_data;
			m_dirty = false;
			lock.unlock();

			const std::string payload = j.dump();
			socket.send(zmq::buffer(payload), zmq::send_flags::dontwait); // drop on EAGAIN, next heartbeat retries

			lock.lock();
			m_cv.wait_for(lock, m_heartbeat, [this] { return m_dirty || !m_running; });
		}
	}
	catch(const zmq::error_t& e)
	{
		std::cerr << "StatusReporter: ZeroMQ error: " << e.what() << std::endl;
	}
}

} // namespace mandeye::extras
