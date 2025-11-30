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

void * GetLibHandle(const std::string& libPath)
{
    void* handle = dlopen(libPath.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!handle)
    {
        std::cerr << dlerror() << std::endl;
    }
    if (handle)
    {
        return handle;
    }
    std::vector<std::string> paths ={
        "/usr/local/lib/",
        "/opt/mandeye/",
        "/home/michal/jcode/mandeye_controller/cmake-build-debug/"
    };
    for (const auto& path : paths)
    {
        std::cerr << "Trying to load from " << path << std::endl;
        handle = dlopen((path + libPath).c_str(), RTLD_LAZY | RTLD_GLOBAL);
        if (!handle)
        {
            std::cerr << dlerror() << std::endl;
        }
        if (handle)
        {
            std::cout << "Loaded from " << path << std::endl;
            return handle;
        }
    }
    std::cerr << "Failed to load from any path" << std::endl;
    std::abort();
    throw std::runtime_error("Failed to load library: " + std::string(dlerror()));
    return nullptr;
}

// Wrapper for a dynamically loaded lidar client
template <typename ClientType>
std::shared_ptr<ClientType> make_dynamic_client(const std::string& libPath,
                                                const std::string& createSym,
                                                const std::string& destroySym)
{
    void* handle = GetLibHandle(libPath);

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
    else if (lidarType == "HESAI")
    {
        try
        {
            return make_dynamic_client<BaseLidarClient>("libhesai_lib.so", "create_hesai_client", "destroy_hesai_client");
        }
        catch (const std::exception& e)
        {
            std::cerr << "[HESAI] " << e.what() << std::endl;
            return nullptr;
        }
    }

    else
    {
        throw std::runtime_error("Unknown lidar type: " + lidarType);
    }
}

} // namespace mandeye