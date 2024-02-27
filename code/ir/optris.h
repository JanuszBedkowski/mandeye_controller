#pragma once

#include <deque>
#include <json.hpp>
#include <mutex>

#include <SerialPort.h>
#include <SerialStream.h>
#include "thread"
#include "utils/TimeStampReceiver.h"
#include "minmea.h"

// Forward declaration, since we do not want to have otpris SDK in headers.
namespace evo
{
class IRDevice;
}

namespace mandeye
{
class OptisClientCallbacks;
class OptisClient : public mandeye_utils::TimeStampReceiver
{
public:
	friend OptisClientCallbacks;
	nlohmann::json produceStatus();

	//! Spins up a thread that reads from the serial port
	bool startListener(const std::string& cameraCalibration);

	void stopListener();


	//! Start logging into the buffers
	void startLog(const std::string& directory);

	//! Stop logging into the buffers
	void stopLog();

	bool isLogging() const {
		return m_isLogging;
	}

	void setCameraTimingSec(double timingSec) {
		m_cameraTimingSec = timingSec;
	}

private:

	void cameraLoop();
	bool m_running = false;
	std::string m_cameraCalibrationFile;
	evo::IRDevice * m_dev {nullptr};
	bool m_isLogging = false;
	std::string m_logDirectory;
	std::thread cameraThread;
	uint64_t m_cameraImages = 0;
	double m_cameraTimingSec = 0.25;
	double m_cameraTimingSecLast = 0.0;
};
} // namespace mandeye