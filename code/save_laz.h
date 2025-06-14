#pragma once
#include <LivoxClient.h>
#include <string>
#include <json.hpp>
namespace mandeye
{
struct LazStats {
    float m_sizeMb{-1.f};
    float m_saveDurationSec2{-1.f};
    float m_saveDurationSec1{-1.f};
    uint64_t m_pointsCount{0};
    std::string m_filename;
    int m_decimationStep{1};
    nlohmann::json produceStatus() const;

};

std::optional<LazStats> saveLaz(const std::string& filename, LivoxPointsBufferPtr buffer);
}