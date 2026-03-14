#pragma once
#include <functional>
#include <string>
#include <nlohmann/json.hpp>
namespace mandeye::extras {

namespace keys {
    constexpr const char* ZMQ_ENDPOINT               = "tcp://localhost:5556";
    constexpr const char* MODE                        = "mode";
    constexpr const char* CONTINUOUS_SCAN_DIRECTORY   = "continousScanDirectory";
    constexpr const char* TIME                        = "time";
    constexpr const char* MODE_SCANNING               = "SCANNING";
    constexpr const char* MODE_STOPPING               = "STOPPING";
} // namespace keys

//! Returns environment variable value or default
std::string getEnvString(const std::string& env, const std::string& def);

using StatusCallback = std::function<void(const nlohmann::json&)>;

//! Connects a ZeroMQ SUB socket to ZMQ_ENDPOINT and calls callback with the raw message string.
//! Blocks forever; aborts on ZMQ error.
void startZeroMQListener(const StatusCallback& callback);

} // namespace mandeye::extras