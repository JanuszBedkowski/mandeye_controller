#pragma once

#include <deque>
#include <json.hpp>
#include <mutex>

#include "utils/TimeStampReceiver.h"
#include "vn100/vn100.h"
#include "LivoxTypes.h"
namespace mandeye
{


class VN100Client : public mandeye_utils::TimeStampReceiver
{
public:

	nlohmann::json produceStatus();
	VN100Client();
	~VN100Client() = default;
	//! Spins up a thread that reads from the serial port
	void startListener(const std::string portName);

	//! Start logging into the buffers
	void startLog();

	//! Stop logging into the buffers
	void stopLog();

	//! Retrieve all data from the buffer, in form of CSV lines
	std::deque<std::string> retrieveData();

private:
	unsigned int count =0;
	double timestamp = 0;
	void dataCallback(double ts, std::array<float, 4> q,
					  std::array<float, 3> acc,
					  std::array<float, 3> gyro);

	std::mutex m_lock;
	std::unique_ptr<vn100_client> m_vn100;
	std::deque<std::string> m_buffer;
	bool m_isLogging{false};

	std::array<float, 3> rotateAroundX(std::array<float, 3> input);
};
} // namespace mandeye