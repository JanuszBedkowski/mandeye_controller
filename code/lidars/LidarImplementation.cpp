#include "LidarImplementations.h"

#include "lidars/dummy/ButterLidar.h"
#include "lidars/livoxsdk2/LivoxClient.h"

// add here all other lidar implementations

namespace mandeye
{

    BaseLidarClientPtr createLidarClient(const std::string& lidarType, const nlohmann::json& config)
    {
        if (lidarType == "LIVOX_SDK2")
        {
            return std::make_shared<LivoxClient>(); // Livox SDK2 implementation
        }

        else if (lidarType == "BUTTER_LIDAR")
        {
            return std::make_shared<ButterLidar>(); // Butter Lidar implementation
        }
        else
        {
            throw std::runtime_error("Unknown lidar type: " + lidarType);
        }
    }
} // namespace mandeye