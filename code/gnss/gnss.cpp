#include "gnss.h"
#include "../3rd/minmea/minmea.h"
#include <exception>
#include <iostream>
#include <thread>

namespace mandeye
{
    std::string sanitize_ascii(const std::string& input) {
        std::string output;
        for (char c : input) {
            // Keep only printable ASCII and basic control chars like \r and \n
            if ((c >= 32 && c <= 126) || c == '\r' || c == '\n') {
                output += c;
            }
        }
        return output;
    }

nlohmann::json GNSSClient::produceStatus()
{
	nlohmann::json data;
	data["init_success"] = init_succes;
	std::lock_guard<std::mutex> lock(m_bufferMutex);
	data["nmea"]["last_line"] = sanitize_ascii(m_lastLine);
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
	data["is_logging"] = m_isLogging;
	data["message_count"] = m_messageCount.load();
	data["buffer_size"] = m_buffer.size();
        data["ntrip_client"]["number_of_rtcm3_messages"] = m_numberOfRTCM3Messages.load();
        data["ntrip_client"]["number_of_gga_messages"] = m_numberOfGGAMessagesToCaster.load();
	return data;
}

bool GNSSClient::startListener(const std::string& portName, LibSerial::BaudRate baudRate) {

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
		m_serialPort.Open(portName, std::ios_base::in|std::ios_base::out);
		m_serialPort.SetBaudRate(baudRate);
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
		{
			std::lock_guard<std::mutex> lock(m_bufferMutex);
			m_lastLine = line;
		}
		bool is_vaild = minmea_check(line.c_str(), true);
		if (is_vaild)
		{
			const double laserTimestamp = GetTimeStamp();
			m_rawbuffer.emplace_back(RawEntryToLine(line, GetTimeStamp()));
			minmea_sentence_gga gga;
			bool isGGA = minmea_parse_gga(&gga, line.c_str());
			if (isGGA)
			{
				if (m_dataCallback)
				{
					m_dataCallback(gga);
				}

				std::string csvline = GgaToCsvLine(gga, laserTimestamp);
				{
				    std::lock_guard<std::mutex> lockLastGGA(m_ggaMutex);
				    lastGGA = gga;
				    m_lastGGARaw = line;
				}
				std::lock_guard<std::mutex> lock(m_bufferMutex);
				std::swap(m_lastLine, line);

				m_messageCount++;
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

}
void GNSSClient::startLog() {
	std::lock_guard<std::mutex> lock(m_bufferMutex);
	m_buffer.clear();
	m_rawbuffer.clear();
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
	return ret;
}

std::deque<std::string> GNSSClient::retrieveRawData()
{
	std::lock_guard<std::mutex> lock(m_bufferMutex);
	std::deque<std::string> ret;
	std::swap(ret, m_rawbuffer);
	return ret;
}
//! Convert a minmea_sentence_gga to a CSV line
std::string GNSSClient::GgaToCsvLine(const minmea_sentence_gga& gga, double laserTimestamp)
{
	auto now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

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
	oss << gga.fix_quality << " ";
	oss << millis.count() << "\n";
	return oss.str();
}

std::string GNSSClient::RawEntryToLine(const std::string& line, double laserTimestamp)
{
	auto now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

	std:std::stringstream oss;
	oss << std::setprecision(20) << static_cast<uint_least64_t>(laserTimestamp * 1000000000.0) << " ";
	oss << millis.count() << " ";
	oss << line << " ";
	return oss.str();
}

void GNSSClient::setDataCallback(const std::function<void(const minmea_sentence_gga& gga)>& callback)
{
	m_dataCallback = callback;
}

void GNSSClient::setNtripClient(
    const std::string& userName,
    const std::string& password,
    const std::string& mountPoint,
    const std::string& host,
    const std::string& port)
{
    std::thread t(
        [this, userName, password, mountPoint, host, port]()
        {
            boost::asio::io_context io;
            m_ntripClient = std::make_unique<NtripClient>(
                io,
                host,
                port,
                mountPoint,
                userName,
                password,
                [this](const uint8_t* data, size_t size)
                {
                    m_numberOfRTCM3Messages++;
                    std::vector<uint8_t> dataVec(data, data + size);
                    if (m_serialPort.IsOpen())
                    {
                        m_serialPort.Write(dataVec);
                    }
                });
            m_ntripClient->start();
            boost::asio::steady_timer timer(io);
            sheduleGgaSend(timer);
            io.run();
        });

    m_ntripThread = std::move(t);
}

void GNSSClient::setNtripClient(const nlohmann::json& ntripClientConfig)
{
    if (!ntripClientConfig.is_object())
    {
        return;
    }

    std::string userName = ntripClientConfig.value("user_name", "");
    std::string password = ntripClientConfig.value("password", "");
    std::string mountPoint = ntripClientConfig.value("mount_point", "");
    std::string host = ntripClientConfig.value("host", "");
    std::string port = ntripClientConfig.value("port", "2101");
    if (userName.empty() || password.empty() || mountPoint.empty() || host.empty() || port.empty())
    {
            std::cerr << "NTRIP client configuration is incomplete." << std::endl;
            return;
    }
    setNtripClient(userName, password, mountPoint, host, port);
}



} // namespace mandeye