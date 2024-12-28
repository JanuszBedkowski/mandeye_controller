#include <chrono>
#include <gpios.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <stdio.h>

#include <SerialPort.h>
#include <SerialStream.h>
#include <hardware_config/mandeye.h>
#include <gpiod.h>
namespace NMEA
{
const unsigned int BufferLen = 128;

//! Helper structure to get time
struct timestamp
{
	uint8_t hours;
	uint8_t mins;
	uint8_t secs;
	uint8_t day;
	uint8_t month;
	uint8_t year;
};

std::string produceNMEA(const NMEA::timestamp& ts)
{
	char buffer[BufferLen];
	char payload[BufferLen];
	snprintf(
		//		payload, NMEA::BufferLen, "GPRMC,%02d%02d%02d.00,A,5109.0262308,N,11401.8407342,W,0.004,133.4,%s,0.0,E,D", ts.hours, ts.mins, ts.secs, date);
		payload,
		NMEA::BufferLen,
		"GPRMC,%02d%02d%02d.00,A,5109.0262308,N,11401.8407342,W,0.004,133.4,%02d%02d%02d,0.0,E,D",
		ts.hours,
		ts.mins,
		ts.secs,
		ts.day,
		ts.month,
		ts.year);
	size_t len = strnlen(payload, NMEA::BufferLen);
	// compute NMEA checksum on buffer
	uint8_t NMEAChecksumComputed = 0;

	size_t i = 0;
	for(i = 0; i < len; i++)
	{
		NMEAChecksumComputed ^= payload[i];
	}
	// attach cheksum
	snprintf(buffer, NMEA::BufferLen, "$%s*%02X\n", payload, NMEAChecksumComputed);
	return std::string(buffer);
}

NMEA::timestamp GetTimestampFromSec(time_t secsElapsed)
{
	std::tm* timeInfo = gmtime(&secsElapsed);
	NMEA::timestamp ts;
	ts.hours = timeInfo->tm_hour;
	ts.mins = timeInfo->tm_min;
	ts.secs = timeInfo->tm_sec;
	ts.day = timeInfo->tm_mday;
	ts.month = timeInfo->tm_mon + 1;
	ts.year = timeInfo->tm_year - 100;
	return ts;
}
} // namespace NMEA

std::atomic<bool> stop{false};
void oneSecondThread()
{
	// setup serial port
	std::vector<std::unique_ptr<LibSerial::SerialPort>> serialPorts;
	std::vector<gpiod_line*> syncOutsLines;

	const auto portsNames = hardware::GetLidarSyncPorts();
	for(const auto& portName : portsNames)
	{
		std::cout << "opening port " << portName << std::endl;
		std::unique_ptr<LibSerial::SerialPort> serialPort = std::make_unique<LibSerial::SerialPort>();
		serialPort->Open(portName, std::ios_base::out);
		serialPort->SetBaudRate(LibSerial::BaudRate::BAUD_9600);
		serialPorts.emplace_back(std::move(serialPort));
	}
	const auto ouputs = hardware::GetLidarSyncLEDs();
	const auto& chipPath = mandeye::GetGPIOChip();
	std::cout << "Opening GPIO chip " << chipPath << std::endl;

	gpiod_chip *chip = gpiod_chip_open(chipPath);
	if (chip == nullptr)
	{
		std::cerr << "Error: Unable to open GPIO chip." << std::endl;
		std::abort();
	}


	for (const auto& led : ouputs)
	{
		auto pin = hardware::GetLED(led);
		auto line = gpiod_chip_get_line(chip, pin);
		if (line == nullptr)
		{
			std::cerr << "Error: Unable to open GPIO line." << std::endl;
			gpiod_chip_close(chip);
			std::abort();
		}
		int ret = gpiod_line_request_output(line, "mandeye_fake_pps", 0);
		if (ret < 0)
		{
			std::cerr << "Error: Unable to request GPIO line." << std::endl;
			gpiod_chip_close(chip);
			std::abort();
		}
		syncOutsLines.emplace_back(line);
	}
	assert(serialPorts.size() == syncOuts.size());

	//setup pps gpio
	constexpr uint64_t Rate = 1000;
	const auto now = std::chrono::system_clock::now();
	uint64_t millisFromEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
	millisFromEpoch += Rate;

	//round to next second
	millisFromEpoch = (millisFromEpoch / Rate) * Rate;
	auto waKeUpTime = std::chrono::system_clock::time_point(std::chrono::milliseconds(millisFromEpoch));

	while(!stop)
	{
		std::this_thread::sleep_until(waKeUpTime);
		auto currentTime = std::chrono::system_clock::now();
		millisFromEpoch += Rate;

		waKeUpTime = std::chrono::system_clock::time_point(std::chrono::milliseconds(millisFromEpoch));

		const uint64_t secs = millisFromEpoch / 1000;
		NMEA::timestamp ts = NMEA::GetTimestampFromSec(secs);

		for (auto& syncOut : syncOutsLines)
		{
			gpiod_line_set_value(syncOut, 0);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		for (auto& syncOut : syncOutsLines)
		{
			gpiod_line_set_value(syncOut, 1);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		const std::string nmeaMessage = NMEA::produceNMEA(ts);
		for (auto& serialPort : serialPorts)
		{
			serialPort->Write(nmeaMessage);
		}

		std::this_thread::sleep_until(waKeUpTime);
	}
	for (auto& syncOut : syncOutsLines)
	{
		gpiod_line_release(syncOut);
	}
	gpiod_chip_close(chip);
}
int main(int arc, char* argv[])
{
	std::cout << "fake pps" << std::endl;

	std::thread t1(oneSecondThread);
	while(!stop)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
	t1.join();
	return 0;
}
