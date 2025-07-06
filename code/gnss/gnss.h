#pragma once

#include <deque>
#include <json.hpp>
#include <mutex>

#include <SerialPort.h>
#include <SerialStream.h>
#include "thread"
#include "utils/TimeStampReceiver.h"
#include "minmea.h"
#include "ntrip.h"
#include <atomic>
#include <iostream>
namespace mandeye
{


class GNSSClient : public mandeye_utils::TimeStampReceiver
{
public:

	nlohmann::json produceStatus();

	//! Spins up a thread that reads from the serial port
	bool startListener(const std::string& portName, LibSerial::BaudRate baudRate);
	bool startListener();
	//! Start logging into the buffers
	void startLog();

	//! Stop logging into the buffers
	void stopLog();

	//! Retrieve all data from the buffer, in form of CSV lines
	std::deque<std::string> retrieveData();

	//! Retrieve all data from the buffer, in form of CSV lines
	std::deque<std::string> retrieveRawData();


	//! Addcallback on data received
	void setDataCallback(const std::function<void(const minmea_sentence_gga& gga)>& callback);

        void sheduleGgaSend(boost::asio::steady_timer& timer)
        {
            timer.expires_after(std::chrono::seconds(1));
            timer.async_wait(
                [this, &timer](const boost::system::error_code& ec)
                {
                    if (!ec)
                    {
                        std::lock_guard<std::mutex> lockLastGGA(m_ggaMutex);
                        m_ntripClient->send_gga(m_lastGGARaw);
                    	m_numberOfGGAMessagesToCaster++;
                    }
                    sheduleGgaSend(timer);
                });
        }

	void setNtripClient(const nlohmann::json& ntripClientConfig);

	//! Set up the NTRIP client to send RTCM3 messages to a caster
        void setNtripClient(const std::string& userName, const std::string& password,
                            const std::string& mountPoint,
                            const std::string& host, const std::string& port);
private:
	std::mutex m_bufferMutex;
	std::deque<std::string> m_buffer;
	std::deque<std::string> m_rawbuffer;

	std::string m_lastLine;
	bool m_isLogging{false};


	std::mutex m_ggaMutex;
	std::string m_lastGGARaw;
	minmea_sentence_gga lastGGA;
	LibSerial::SerialPort m_serialPort;
	std::thread m_serialPortThread;
	std::string m_portName;

        std::atomic<uint64_t> m_numberOfRTCM3Messages{0};
	std::atomic<uint64_t> m_numberOfGGAMessagesToCaster{0};
	int m_baudRate {0};
	void worker();

	bool init_succes{false};

	//! Convert a minmea_sentence_gga to a CSV line
	std::string GgaToCsvLine(const minmea_sentence_gga& gga, double laserTimestamp);

	//! Convert a raw entry to a CSV line
	std::string RawEntryToLine(const std::string& line, double laserTimestamp);
	//! Callbacks to call when new data is received
	std::function<void(const minmea_sentence_gga& gga)> m_dataCallback;
	std::atomic<uint32_t> m_messageCount{0};
    std::unique_ptr<mandeye::NtripClient> m_ntripClient;
    std::thread m_ntripThread;

};
} // namespace mandeye