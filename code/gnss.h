#pragma once

#include <deque>
#include <json.hpp>
#include <mutex>

#include <SerialPort.h>
#include <SerialStream.h>
#include "thread"
#include "utils/TimeStampReceiver.h"
#include "minmea.h"
#include <atomic>
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

private:
	std::mutex m_bufferMutex;
	std::deque<std::string> m_buffer;
	std::deque<std::string> m_rawbuffer;

	std::string m_lastLine;
	bool m_isLogging{false};
	minmea_sentence_gga lastGGA;
	LibSerial::SerialPort m_serialPort;
	LibSerial::SerialStream m_serialPortStream;
	std::thread m_serialPortThread;
	std::string m_portName;
	int m_baudRate {0};
	void worker();

	bool init_succes{false};

	//! Convert a minmea_sentence_gga to a CSV line
	std::string GgaToCsvLine(const minmea_sentence_gga& gga, double laserTimestamp);

	//! Convert a raw entry to a CSV line
	std::string RawEntryToLine(const std::string& line, double laserTimestamp);
	//! Callbacks to call when new data is received
	std::function<void(const minmea_sentence_gga& gga)> m_dataCallback;
	std::atomic<unsigned uint32_t> m_messageCount{0};
};
} // namespace mandeye