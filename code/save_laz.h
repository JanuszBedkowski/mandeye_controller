#pragma once
#include <LivoxClient.h>
#include <string>
namespace mandeye
{
bool saveLaz(const std::string& filename, LivoxPointsBufferPtr buffer);
}