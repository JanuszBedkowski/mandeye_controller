#include "extras_monitor.h"
#include <cmath>
#include <iostream>

namespace mandeye
{
ExtrasMonitor::ExtrasMonitor()
	: m_context(1)
	, m_socket(m_context, zmq::socket_type::pull)
{
	m_socket.set(zmq::sockopt::linger, 0);
	m_socket.set(zmq::sockopt::rcvtimeo, 200); // lets the worker notice shutdown
	m_socket.bind(BIND_ENDPOINT);
	m_thread = std::thread(&ExtrasMonitor::worker, this);
}

ExtrasMonitor::~ExtrasMonitor()
{
	m_running = false;
	if(m_thread.joinable())
	{
		m_thread.join();
	}
	m_socket.close();
}

void ExtrasMonitor::worker()
{
	while(m_running)
	{
		zmq::message_t message;
		try
		{
			if(!m_socket.recv(message, zmq::recv_flags::none))
			{
				continue; // timeout
			}
		}
		catch(const zmq::error_t& e)
		{
			std::cerr << "ExtrasMonitor: ZeroMQ error: " << e.what() << std::endl;
			continue;
		}
		handleMessage(std::string(static_cast<const char*>(message.data()), message.size()));
	}
}

void ExtrasMonitor::handleMessage(const std::string& payload)
{
	const auto j = nlohmann::json::parse(payload, nullptr, false);
	if(!j.is_object() || !j.contains("service") || !j["service"].is_string() || !j.contains("ok") || !j["ok"].is_boolean())
	{
		std::cerr << "ExtrasMonitor: ignoring malformed report: " << payload << std::endl;
		return;
	}
	const std::string name = j["service"].get<std::string>();
	const bool ok = j["ok"].get<bool>();
	const std::string msg = j.value("message", std::string{});

	const auto now = std::chrono::steady_clock::now();
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_services.find(name);
	const bool isNew = it == m_services.end();
	if(isNew)
	{
		it = m_services.emplace(name, ServiceState{}).first;
	}
	ServiceState& s = it->second;
	const bool wasHealthy = !isNew && s.ok && !isStale(s, now);
	if(isNew || wasHealthy != ok || (!ok && s.message != msg))
	{
		std::cout << "ExtrasMonitor: service '" << name << "' " << (ok ? "OK" : "FAILED") << (msg.empty() ? "" : ": " + msg) << std::endl;
	}
	s.ok = ok;
	s.message = msg;
	s.data = j.contains("data") ? j["data"] : nlohmann::json::object();
	if(j.contains("timeout_s") && j["timeout_s"].is_number() && j["timeout_s"].get<double>() > 0)
	{
		s.timeoutS = j["timeout_s"].get<double>();
	}
	s.lastSeen = now;
}

bool ExtrasMonitor::isStale(const ServiceState& s, std::chrono::steady_clock::time_point now)
{
	return std::chrono::duration<double>(now - s.lastSeen).count() > s.timeoutS;
}

nlohmann::json ExtrasMonitor::produceStatus() const
{
	const auto now = std::chrono::steady_clock::now();
	nlohmann::json services = nlohmann::json::object();
	bool allOk = true;
	std::lock_guard<std::mutex> lock(m_mutex);
	for(const auto& [name, s] : m_services)
	{
		const bool stale = isStale(s, now);
		const bool ok = s.ok && !stale;
		allOk = allOk && ok;
		nlohmann::json e;
		e["ok"] = ok;
		e["stale"] = stale;
		e["message"] = stale ? "no report for more than " + std::to_string(static_cast<int>(s.timeoutS)) + " s" : s.message;
		e["age_s"] = std::round(std::chrono::duration<double>(now - s.lastSeen).count() * 10) / 10.0;
		e["data"] = s.data;
		services[name] = e;
	}
	return {{"all_ok", allOk}, {"services", services}};
}

bool ExtrasMonitor::hasFailure() const
{
	return !produceStatus()["all_ok"].get<bool>();
}
} // namespace mandeye
