#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
namespace mandeye::extras
{

namespace keys
{
constexpr const char* ZMQ_ENDPOINT = "tcp://localhost:5556";
//! Extras -> controller health reports (controller binds tcp://*:5557, see code/extras_monitor.h)
constexpr const char* ZMQ_STATUS_ENDPOINT = "tcp://localhost:5557";
constexpr const char* MODE = "mode";
constexpr const char* CONTINUOUS_SCAN_DIRECTORY = "continousScanDirectory";
constexpr const char* TIME = "time";
constexpr const char* MODE_SCANNING = "SCANNING";
constexpr const char* MODE_STOPPING = "STOPPING";
} // namespace keys

//! Returns environment variable value or default
std::string getEnvString(const std::string& env, const std::string& def);

using StatusCallback = std::function<void(const nlohmann::json&)>;

//! Connects a ZeroMQ SUB socket to ZMQ_ENDPOINT and calls callback with the raw message string.
//! Blocks forever; aborts on ZMQ error.
void startZeroMQListener(const StatusCallback& callback);

//! Reports this service's health back to the controller (shown in /json/status under "extras").
//! Sends the current state immediately on change and re-sends it every heartbeat interval, so the controller
//! can also detect a service that died or hung (no report within timeout => reported as failed).
//! Safe to call from any thread. Never blocks; if the controller is not running, only the latest state is kept.
//!
//! Usage:
//!   mandeye::extras::StatusReporter status("murata_imu");
//!   status.reportFailure("Failed to open /dev/ttyAMA2");
//!   ...
//!   status.reportOk();
//!
//! Wire format (single ZMQ PUSH frame, usable from Python etc.):
//!   {"service": "<name>", "ok": <bool>, "message": "<text>", "timeout_s": <double>, "data": {...}}
class StatusReporter
{
public:
	explicit StatusReporter(std::string serviceName,
							std::chrono::milliseconds heartbeat = std::chrono::seconds(1),
							std::chrono::milliseconds timeout = std::chrono::seconds(5));
	~StatusReporter();
	StatusReporter(const StatusReporter&) = delete;
	StatusReporter& operator=(const StatusReporter&) = delete;

	void reportOk(const std::string& message = "");
	void reportFailure(const std::string& message);
	//! Full report; `data` is free-form service-specific detail (counters, timestamps, ...).
	void report(bool ok, const std::string& message, const nlohmann::json& data = nlohmann::json::object());

private:
	void worker();

	const std::string m_serviceName;
	const std::chrono::milliseconds m_heartbeat;
	const std::chrono::milliseconds m_timeout;
	std::mutex m_mutex;
	std::condition_variable m_cv;
	bool m_ok = true;
	std::string m_message = "starting";
	nlohmann::json m_data = nlohmann::json::object();
	bool m_dirty = true;
	bool m_running = true;
	std::thread m_thread;
};

} // namespace mandeye::extras