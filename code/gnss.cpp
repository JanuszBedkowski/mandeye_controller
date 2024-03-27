#include "gnss.h"
#include <iostream>
#include <thread>
#include <exception>
#include "minmea.h"
namespace mandeye
{
nlohmann::json GNSSClient::produceStatus()
{
	nlohmann::json data;
	data["init_success"] = init_succes;
	std::lock_guard<std::mutex> lock(m_bufferMutex);
	data["nmea"]["last_line"] = m_lastLine;
	data["gga"]["time"]["h"] = lastGGA.time.hours;
	data["gga"]["time"]["m"] = lastGGA.time.minutes;
	data["gga"]["time"]["s"] = lastGGA.time.seconds;
	data["gga"]["fix_quality"] = lastGGA.fix_quality;
	data["gga"]["satellites_tracked"] = lastGGA.satellites_tracked;
	data["gga"]["latitude"] = minmea_tocoord(&lastGGA.latitude);
	data["gga"]["longitude"] = minmea_tocoord(&lastGGA.longitude);
	data["gga"]["hdop"] = minmea_tofloat(&lastGGA.hdop);
	data["gga"]["satellites_tracked"] = lastGGA.satellites_tracked;
	data["gga"]["altitude"] = minmea_tofloat(&lastGGA.altitude);
	data["gga"]["height"] = minmea_tofloat(&lastGGA.height);
	data["gga"]["dgps_age"] = minmea_tofloat(&lastGGA.dgps_age);
	return data;
}

bool GNSSClient::startListener(const std::string& portName, int baudRate) {
	assert(baudRate == 9600);//Only 9600 is supported
	try
	{
		if (init_succes)
		{
			return true;
		}
		if (m_serialPort.IsOpen())
		{
			m_serialPort.Close();
		}
		m_serialPort.Open(portName, std::ios_base::in);
		m_serialPort.SetBaudRate(LibSerial::BaudRate::BAUD_9600);
		init_succes = true;
		m_serialPortThread = std::thread(&GNSSClient::worker, this);
	}catch(std::exception& e)
	{
		std::cout << "Failed to open port " << portName <<" : " << e.what()  << std::endl;
		init_succes = false;
		return false;
	}
	return true;
}

void GNSSClient::worker()
{
	std::cout << "Worker started" << std::endl;
	while(m_serialPort.IsOpen())
	{
		std::string line;
		m_serialPort.ReadLine(line);
		std::cout << "line: '" << line << "'" << std::endl;

		bool is_vaild = minmea_check(line.c_str(), true);
		if (is_vaild)
		{
			minmea_sentence_gga gga;
			bool isGGA = minmea_parse_gga(&gga, line.c_str());
			if (isGGA)
			{
				double laserTimestamp = GetTimeStamp();
				std::string csvline = GgaToCsvLine(gga, laserTimestamp);
				std::lock_guard<std::mutex> lock(m_bufferMutex);
				std::swap(m_lastLine, line);
				lastGGA = gga;
				if(m_isLogging)
				{
					m_buffer.emplace_back(csvline);
				}
			}
		}
		else
		{
			std::cout << "Invalid line: " << line << std::endl;
		}
	}
	//std::cout << "problem with GNSS" << std::endl;
	//exit(1);
}
void GNSSClient::startLog() {
	std::lock_guard<std::mutex> lock(m_bufferMutex);
	m_isLogging = true;
}

void GNSSClient::stopLog() {
	std::lock_guard<std::mutex> lock(m_bufferMutex);
	m_isLogging = false;
}

std::deque<std::string> GNSSClient::retrieveData()
{
	std::lock_guard<std::mutex> lock(m_bufferMutex);
	std::deque<std::string> ret;
	std::swap(ret, m_buffer);

	//if(ret.size() == 0){
	//	if(m_serialPort.IsOpen()){
	//		m_serialPort.Close();
	//		m_serialPort.Open("/dev/ttyS0");
	//	}
	//}

	return ret;
}

//! Convert a minmea_sentence_gga to a CSV line
std::string GNSSClient::GgaToCsvLine(const minmea_sentence_gga& gga, double laserTimestamp)
{
	std:std::stringstream oss;
	oss << std::setprecision(20) << static_cast<uint_least64_t>(laserTimestamp * 1000000000.0) << " ";
	oss << minmea_tocoord(&gga.latitude) << " ";
	oss	<< minmea_tocoord(&gga.longitude) << " ";
	oss	<< minmea_tofloat(&gga.altitude) << " ";
	oss << minmea_tofloat(&gga.hdop) << " ";
	oss << gga.satellites_tracked << " ";
	oss << minmea_tofloat(&gga.height) << " ";
	oss << minmea_tofloat(&gga.dgps_age) << " ";
	oss << gga.time.hours << ":" << gga.time.minutes << ":" << gga.time.seconds << " ";
	oss << gga.fix_quality << "\n";
	return oss.str();
}
} // namespace mandeye