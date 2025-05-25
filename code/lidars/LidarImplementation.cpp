#include "LidarImplementations.h"

#include "lidars/dummy/ButterLidar.h"

#ifdef MANDEYE_USE_LIVOX_SDK2
#include "lidars/livoxsdk2/LivoxClient.h"
#endif

#ifdef MANDEYE_USE_OUSTER_SDK
#include "lidars/ouster/OusterClient.h"
#endif

#include <iostream>

// add here all other lidar implementations

namespace mandeye
{

    BaseLidarClientPtr createLidarClient(const std::string& lidarType, const nlohmann::json& config)
    {

        if (lidarType == "LIVOX_SDK2")
        {
#ifdef MANDEYE_USE_LIVOX_SDK2
            return std::make_shared<LivoxClient>(); // Livox SDK2 implementation
#endif
            std::cerr << "LIVOX_SDK2 was not active during compilation" << std::endl;
            return nullptr;
        }

        else if (lidarType == "BUTTER_LIDAR")
        {
            return std::make_shared<ButterLidar>(); // Butter Lidar implementation
        }
        else if (lidarType == "OUSTER")
        {
#ifdef MANDEYE_USE_OUSTER_SDK
            return std::make_shared<OusterClient>(); // Ouster lidar implementation
#endif
            std::cerr << "Ouster lidar was not active during compilation" << std::endl;
            return nullptr;
        }

        else
        {
            throw std::runtime_error("Unknown lidar type: " + lidarType);
        }
    }
} // namespace mandeye