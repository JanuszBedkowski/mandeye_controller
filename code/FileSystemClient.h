#pragma once

#include "livox_lidar_def.h"
#include <json.hpp>
#include <mutex>
#include <deque>
#include <string>

namespace mandeye {

    class FileSystemClient {
        constexpr static char manifestFilename[] {"mandala_manifest.txt"};
    public:
        FileSystemClient(const std::string& repository);
        nlohmann::json produceStatus();

        //! Test is writable
        float CheckAvailableSpace();

        //! Create Counter file
        int32_t GetIdFromManifest();

        //! Create Counter file
        int32_t GetNextIdFromManifest();


        //! Get is writable
        bool GetIsWritable();

        std::vector<std::string> GetDirectories();

        std::string CreateDirectoryForExperiment();


    private:
        int32_t  m_nextId {0};
        std::string ConvertToText(float mb);
        std::string m_repository;
        std::string m_error;
        std::mutex m_mutex;

    };
} // namespace mandeye