#pragma once
#include "lidars/BaseLidarClient.h"
#include <string>
namespace mandeye
{
bool saveLaz(const std::string& filename, LidarPointsBufferPtr buffer);
}