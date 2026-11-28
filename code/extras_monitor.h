#pragma once
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <zmq.hpp>

namespace mandeye
{
//! Collects health reports sent by the extra services (camera, GNSS, IMU, ...) back to the controller.
//! Extras PUSH JSON messages to tcp://localhost:5557 (see extras/utils/ExtrasUtils.h, StatusReporter):
//!   {"service": "murata_imu", "ok": false, "message": "Failed to open /dev/ttyAMA2", "timeout_s": 5.0, "data": {...}}
//! "service" and "ok" are required; "message", "timeout_s" and "data" (free-form detail, passed through) are optional.
//! A service that stops sending for longer than its timeout is reported as failed (stale).
//! Singleton: one socket bound per process; created (and bound) on the first instance() call.
class ExtrasMonitor
{
public:
	static constexpr const char* BIND_ENDPOINT = "tcp://*:5557";
	static constexpr double DEFAULT_TIMEOUT_S = 5.0;

	static ExtrasMonitor& instance();

	~ExtrasMonitor();
	ExtrasMonitor(const ExtrasMonitor&) = delete;
	ExtrasMonitor& operator=(const ExtrasMonitor&) = delete;

	//! {"all_ok": bool, "services": {name: {"ok", "message", "age_s", "stale", "data"}}}
	nlohmann::json produceStatus() const;

	//! True if any service that has ever reported is currently failed or stale.
	bool hasFailure() const;

private:
	ExtrasMonitor();

	struct ServiceState
	{
		bool ok = false;
		std::string message;
		nlohmann::json data = nlohmann::json::object();
		double timeoutS = DEFAULT_TIMEOUT_S;
		std::chrono::steady_clock::time_point lastSeen;
	};
	void worker();
	void handleMessage(const std::string& payload);
	static bool isStale(const ServiceState& s, std::chrono::steady_clock::time_point now);

	std::atomic<bool> m_running{true};
	zmq::context_t m_context;
	zmq::socket_t m_socket;
	mutable std::mutex m_mutex;
	std::map<std::string, ServiceState> m_services;
	std::thread m_thread;
};
} // namespace mandeye
