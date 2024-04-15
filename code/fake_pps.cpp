#include <chrono>
#include <cppgpio.hpp>
#include <cppgpio/output.hpp>
#include <gpios.h>
#include <iostream>
#include <thread>

#include <stdio.h>

#include <SerialPort.h>
#include <SerialStream.h>

namespace NMEA
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
	// start time set to Sunday, September 13, 2020 12:26:40 PM - posix is 1600000000.
	const char date[] = "130920"; //dd/mm/yy
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
	LibSerial::SerialPort serialPort;
	serialPort.Open("/dev/ttyS0", std::ios_base::out);
	serialPort.SetBaudRate(LibSerial::BaudRate::BAUD_9600);

	//setup pps gpio
	GPIO::DigitalOut pps(18);
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

		pps.off();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		pps.on();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		const std::string nmeaMessage = NMEA::produceNMEA(ts);
		serialPort.Write(nmeaMessage);

		std::this_thread::sleep_until(waKeUpTime);
	}
}
int main(int arc, char* argv[])
{
	std::cout << "fake pps" << std::endl;

	std::thread t1(oneSecondThread);
	while(!stop)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		//		std::cout << "fake pps" << std::endl;
	}
	t1.join();
	return 0;
}
