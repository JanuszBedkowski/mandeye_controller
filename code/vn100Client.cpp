#include <cppgpio.hpp>
#include <cppgpio/output.hpp>
#include <gpios.h>
#include <iostream>
#include "vn100Client.h"
#include <assert.h>
namespace mandeye
{
VN100Client::VN100Client()
{
	std::cout <<"VN100Client::VN100Client()" << std::endl;
}
nlohmann::json VN100Client::produceStatus()
{
	nlohmann::json data;
	std::lock_guard<std::mutex> lck{m_lock};
	if (m_vn100)
	{
		data["vn100"]["count"] = count;
		data["vn100"]["timestamp"] = timestamp;
	}
	return data;
}


//! Spins up a thread that reads from the serial port
void VN100Client::startListener(const std::string portName)
{
	std::lock_guard<std::mutex> lck{m_lock};
	m_vn100 = std::make_unique<vn100_client>(portName);
	m_vn100->setHandler_data(std::bind(&VN100Client::dataCallback, this,
									   std::placeholders::_1,
									   std::placeholders::_2,
									   std::placeholders::_3,
									   std::placeholders::_4));

}

//! Start logging into the buffers
void VN100Client::startLog()
{
	std::lock_guard<std::mutex> lck{m_lock};
	m_isLogging = true;
}

//! Retrieve all data from the buffer, in form of CSV lines
std::deque<std::string> retrieveData();

//! Stop logging into the buffers
void VN100Client::stopLog()
{
	std::lock_guard<std::mutex> lck{m_lock};
	m_isLogging = false;
}

void VN100Client::dataCallback(double ts, std::array<float, 4> q,
				  std::array<float, 3> acc,
				  std::array<float, 3> gyro)
{

	gyro = rotateAroundX(gyro);
	acc = rotateAroundX(acc);
	double laserTimestamp = GetTimeStamp();
	if (m_isLogging)
	{
		std::stringstream  ss;
		ss <<  uint64_t (laserTimestamp) <<" " << gyro[0] << " " << gyro[1] << " " << gyro[2] <<" "<< acc[0] << " " << acc[1] << " " << acc[2] << std::endl;
		std::lock_guard<std::mutex> lck{m_lock};
		m_buffer.emplace_back(std::move(ss.str()));
	}

	std::lock_guard<std::mutex> lck{m_lock};
	this->timestamp = ts;
	this->count ++;

}

std::deque<std::string> VN100Client::retrieveData()
{
	std::lock_guard<std::mutex> lock(m_lock);
	std::deque<std::string> ret;
	std::swap(ret, m_buffer);
	return ret;
}


std::array<float, 3> VN100Client::rotateAroundX(std::array<float, 3> input)
{
	std::array<float, 3> ret;
	double cosAngle = -1.0; // cos(180 degrees) = -1
	double sinAngle = 0.0;  // sin(180 degrees) = 0
	ret[0] = input[0];
	ret[1] = input[1] * cosAngle - input[2] * sinAngle;
	ret[2] = input[1] * sinAngle + input[2] * cosAngle;
	return ret;
}


} // namespace mandeye