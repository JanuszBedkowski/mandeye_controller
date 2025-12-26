#pragma once

#include <deque>
#include <json.hpp>
#include <mutex>

#include <SerialPort.h>
#include <SerialStream.h>
#include <thread>
#include "utils/TimeStampReceiver.h"
#include "minmea.h"
#include "ntrip.h"
#include <atomic>
#include <iostream>
namespace mandeye
{


class GNSSClient
{
public:

	nlohmann::json produceStatus();

	//! Spins up a thread that reads from the serial port
	bool startListener(const std::string& portName, LibSerial::BaudRate baudRate);
	bool startListener();

        void setDataCallback(const std::function<void(const std::string& line)>& callback);

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

        void setLaserTimestamp(uint64_t laserTimestamp);

private:

        std::mutex m_laserTsMutex;
        uint64_t m_laserTimestamp{0};

	std::mutex m_bufferMutex;
	std::string m_lastLine;


	std::mutex m_ggaMutex;
	std::string m_lastGGARaw;
	minmea_sentence_gga lastGGA;
	LibSerial::SerialPort m_serialPort;
	std::thread m_serialPortThread;
	std::string m_portName;

        std::atomic<uint64_t> m_numberOfRTCM3Messages{0};
	std::atomic<uint64_t> m_numberOfGGAMessagesToCaster{0};
	LibSerial::BaudRate m_baudRate {0};
	void worker();

	bool init_succes{false};

	//! Convert a minmea_sentence_gga to a CSV line
	std::string GgaToCsvLine(const minmea_sentence_gga& gga, uint64_t laserTimestamp);

	//! Convert a raw entry to a CSV line
	std::string RawEntryToLine(const std::string& line, uint64_t laserTimestamp);
	//! Callbacks to call when new data is received
	std::function<void(const std::string& line)> m_dataCallback;
	std::atomic<uint32_t> m_messageCount{0};
        std::unique_ptr<mandeye::NtripClient> m_ntripClient;
        std::thread m_ntripThread;

};
} // namespace mandeye