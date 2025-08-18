#include "LidarImplementations.h"
#include "lidars/dummy/ButterLidar.h"

#include <dlfcn.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace mandeye {

namespace {

// Signature of the SDK client factory
using CreateFunc = void*();
using DestroyFunc = void(void*);

// Wrapper for a dynamically loaded lidar client
template <typename ClientType>
std::shared_ptr<ClientType> make_dynamic_client(const std::string& libPath,
                                                const std::string& createSym,
                                                const std::string& destroySym)
{
    std::string effectiveLibPath = libPath;
    void* handle = dlopen(libPath.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        std::cerr << "Failed to load library: " << dlerror() << std::endl;
    }
    if (!handle) {
        //try again with global install path
        effectiveLibPath = "/opt/mandeye/" + libPath;
        handle = dlopen(effectiveLibPath.c_str(), RTLD_LAZY | RTLD_GLOBAL);
        if (!handle) {
            std::cerr << "Failed to load library from global install path: " << dlerror() << std::endl;
        }
    }

    if (!handle) {
        //try executable directory
        std::filesystem::path exe_path = std::filesystem::canonical("/proc/self/exe"); // Linux only
        std::filesystem::path parent_path = exe_path.parent_path();
        effectiveLibPath = parent_path / libPath;
        handle = dlopen(effectiveLibPath.c_str(), RTLD_LAZY | RTLD_GLOBAL);

        if (!handle) {
            std::cerr << "Failed to load library: " << dlerror() << " from " << (parent_path / libPath) << std::endl;
            std::cerr << "Am I giving up." << std::endl;
            throw std::runtime_error("Failed to load library: " + std::string(dlerror()));
        }
    }

    if (handle)
    {
        std::cout << "Successfully loaded library: " << effectiveLibPath << std::endl;
    }
    auto create = reinterpret_cast<CreateFunc*>(dlsym(handle, createSym.c_str()));
    auto destroy = reinterpret_cast<DestroyFunc*>(dlsym(handle, destroySym.c_str()));

    if (!create || !destroy) {
        dlclose(handle);
        throw std::runtime_error("Failed to load symbols from " + libPath);
    }

    ClientType* instance = static_cast<ClientType*>(create());
    if (!instance) {
        dlclose(handle);
        throw std::runtime_error("Failed to create client from " + libPath);
    }

    // Wrap in shared_ptr with custom deleter that also closes handle
    return std::shared_ptr<ClientType>(instance, [destroy, handle](ClientType* ptr) {
        destroy(ptr);
        dlclose(handle);
    });
}

} // anonymous namespace

BaseLidarClientPtr createLidarClient(const std::string& lidarType, const nlohmann::json& config)
{
    if (lidarType == "LIVOX_SDK2")
    {
        try {
            return make_dynamic_client<BaseLidarClient>(
                "liblivox2_lib.so",
                "create_livox_client",
                "destroy_livox_client"
            );
        } catch (const std::exception& e) {
            std::cerr << "[LIVOX_SDK2] " << e.what() << std::endl;
            return nullptr;
        }
    }

    else if (lidarType == "BUTTER_LIDAR")
    {
        return std::make_shared<ButterLidar>();
    }

    else if (lidarType == "OUSTER")
    {
        try {
            return make_dynamic_client<BaseLidarClient>(
                "libouster_lib.so",
                "create_ouster_client",
                "destroy_ouster_client"
            );
        } catch (const std::exception& e) {
            std::cerr << "[OUSTER] " << e.what() << std::endl;
            return nullptr;
        }
    }
    else if (lidarType == "SICK")
    {
        try {
            return make_dynamic_client<BaseLidarClient>(
                "libsick_lib.so",
                "create_sick_client",
                "destroy_sick_client"
            );
        } catch (const std::exception& e) {
            std::cerr << "[SICK] " << e.what() << std::endl;
            return nullptr;
        }
    }
    else if (lidarType == "ROS2")
    {
        try {
            return make_dynamic_client<BaseLidarClient>(
                "libros2_lib.so",
                "create_client",
                "destroy_client"
            );
        } catch (const std::exception& e) {
            std::cerr << "[ROS2] " << e.what() << std::endl;
            return nullptr;
        }
    }
    else if (lidarType == "DUMMY")
    {
        return std::make_shared<ButterLidar>();
    }
    else
    {
        throw std::runtime_error("Unknown lidar type: " + lidarType);
    }
}

} // namespace mandeye