#pragma once
#include <stdint.h>
#include <deque>
#include <memory>
#include <json.hpp>

namespace mandeye
{
struct LivoxPoint
{
	int32_t x;            /**< X axis, Unit:mm */
	int32_t y;            /**< Y axis, Unit:mm */
	int32_t z;            /**< Z axis, Unit:mm */
	uint8_t reflectivity; /**< Reflectivity */
	uint8_t tag;          /**< Tag */
	uint64_t timestamp;
	uint8_t line_id;
	uint16_t laser_id;
};

struct LivoxIMU
{
	float gyro_x;
	float gyro_y;
	float gyro_z;
	float acc_x;
	float acc_y;
	float acc_z;
	uint64_t timestamp;
	uint16_t laser_id;
};

using LivoxPointsBuffer = std::deque<LivoxPoint>;
using LivoxPointsBufferPtr = std::shared_ptr<std::deque<LivoxPoint>>;
using LivoxPointsBufferConstPtr = std::shared_ptr<const std::deque<LivoxPoint>>;

using LivoxIMUBuffer = std::deque<LivoxIMU>;
using LivoxIMUBufferPtr = std::shared_ptr<std::deque<LivoxIMU>>;
using LivoxIMUBufferConstPtr = std::shared_ptr<const std::deque<LivoxIMU>>;

}