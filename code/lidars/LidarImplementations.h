#include "lidars/BaseLidarClient.h"

#pragma once

namespace mandeye
{
    //! Factory that gives you a BaseLidarClientPtr for a given lidar type
    BaseLidarClientPtr createLidarClient(const std::string& lidarType, const nlohmann::json& config);
} // namespace mandeye