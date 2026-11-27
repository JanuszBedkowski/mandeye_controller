#pragma once
#include "lidars/BaseLidarClient.h"
#include "save_laz.h"
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>

namespace mandeye
{
std::pair<std::string, std::optional<LazStats>> savePointcloudData(LidarPointsBufferPtr buffer, const std::string& directory, int chunk);
void saveLidarList(const std::unordered_map<uint32_t, std::string>& lidars, const std::string& directory, int chunk);
void saveImuData(LidarIMUBufferPtr buffer, const std::string& directory, int chunk);
void saveGnssData(std::deque<std::string>& buffer, const std::string& directory, int chunk);
void saveGnssRawData(std::deque<std::string>& buffer, const std::string& directory, int chunk);
//! Path of the raw lidar UDP capture of a chunk, next to lidarNNNN.laz
std::string getPcapFilePath(const std::string& directory, int chunk);
} // namespace mandeye