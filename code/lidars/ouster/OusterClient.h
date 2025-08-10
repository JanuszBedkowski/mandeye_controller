#pragma once

#include "lidars/BaseLidarClient.h"

#include <json.hpp>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>


namespace mandeye
{

// Forward declarations to avoid exposing OusterSDK types
class OusterClientImpl;

class OusterClient : public BaseLidarClient
{
public:
    OusterClient();
    ~OusterClient() override;

    // BaseLidarClient interface implementation
    void Init(const nlohmann::json& config) override;
    nlohmann::json produceStatus() override;
    bool startListener(const std::string& interfaceIp) override;
    void startLog() override;
    void stopLog() override;
    std::pair<LidarPointsBufferPtr, LidarIMUBufferPtr> retrieveData() override;
    std::unordered_map<uint32_t, std::string> getSerialNumberToLidarIdMapping() const override;

    // TimeStampProvider overrides
    double getTimestamp() override;
    double getSessionDuration() override;
    double getSessionStart() override;
    void initializeDuration() override;

private:
    // Use PIMPL pattern to hide OusterSDK dependencies
    std::unique_ptr<OusterClientImpl> m_impl;
};


} // namespace mandeye

extern "C" void* create_ouster_client() {
    return new mandeye::OusterClient();
}

extern "C" void destroy_ouster_client(void* ptr) {
    delete static_cast<mandeye::OusterClient*>(ptr);
}
