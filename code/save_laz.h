#pragma once
#include <string>
#include <LivoxClient.h>
namespace mandeye{
    bool saveLaz (const std::string &filename, LivoxPointsBufferPtr buffer);
}