#include <atomic>
#include <chrono>
#include <cmath>
#include <gpios.h>
#include <iostream>
#include <stdio.h>
#include <thread>
#include <sched.h>
#include <sys/mman.h>
#include <SerialPort.h>
#include <SerialStream.h>
#include <gpiod.h>
#include <hardware_config/mandeye.h>

#include "hardware_config/mandeye.h"

struct JitterStats
{
	long long minUs = std::numeric_limits<long long>::max();
	long long maxUs = std::numeric_limits<long long>::min();
	double sumUs = 0.0;
	double sumSqUs = 0.0;
	uint64_t count = 0;

	void update(long long us)
	{
		minUs = std::min(minUs, us);
		maxUs = std::max(maxUs, us);
		sumUs += us;
		sumSqUs += static_cast<double>(us) * us;
		++count;
	}

	void report(long long lastUs) const
	{
		if(count == 0) return;
		double mean = sumUs / count;
		double variance = (sumSqUs / count) - (mean * mean);
		double stddev = variance > 0.0 ? std::sqrt(variance) : 0.0;
		printf("PPS jitter [us] last=%+lld  min=%lld  max=%lld  mean=%.1f  stddev=%.1f  n=%llu\n",
			   lastUs, minUs, maxUs, mean, stddev, (unsigned long long)count);
		fflush(stdout);
	}
};

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
		"GPRMC,%02d%02d%02d.00,A,5109.038,N,11401.000,W,000.0,000.0,%02d%02d%02d,000.0,W",
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
	snprintf(buffer, NMEA::BufferLen, "$%s*%02X\r\n", payload, NMEAChecksumComputed);
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
	const auto ouputs = hardware::GetLidarSyncGPIO();
	const auto& chipPath = hardware::GetGPIOChip();
	std::cout << "Opening GPIO chip " << chipPath << std::endl;

	gpiod_chip* chip = gpiod_chip_open(chipPath);
	if(chip == nullptr)
	{
		std::cerr << "Error: Unable to open GPIO chip." << std::endl;
		std::abort();
	}

	for(const auto& pin : ouputs)
	{
		auto line = gpiod_chip_get_line(chip, pin);
		if(line == nullptr)
		{
			std::cerr << "Error: Unable to open GPIO line." << std::endl;
			gpiod_chip_close(chip);
			std::abort();
		}
		int ret = gpiod_line_request_output(line, "mandeye_fake_pps", 0);
		if(ret < 0)
		{
			std::cerr << "Error: Unable to request GPIO line." << std::endl;
			gpiod_chip_close(chip);
			std::abort();
		}
		syncOutsLines.emplace_back(line);
	}
	assert(serialPorts.size() == syncOutsLines.size());

	constexpr uint64_t Rate = 1000;
	JitterStats jitter;

	while(!stop)
	{
		auto currentTime = std::chrono::system_clock::now();

		// get deadline for next second
		const auto millisFromEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime.time_since_epoch()).count();
		const auto nextMillisFromEpoch = ((millisFromEpoch / Rate) + 1) * Rate;

		const auto waKeUpTime = std::chrono::system_clock::time_point(std::chrono::milliseconds(nextMillisFromEpoch));
				// Sleep until spinMarginUs before the deadline, then busy-spin for precision.
		// Spin margin must exceed worst-case sleep overrun (~15us observed).
		constexpr int64_t spinMarginUs = 200;
		std::this_thread::sleep_until(waKeUpTime - std::chrono::microseconds(spinMarginUs));
		while(std::chrono::system_clock::now() < waKeUpTime)
		{
			asm volatile("" ::: "memory"); // prevent loop from being optimized away
		}

		

		const auto actualTime = std::chrono::system_clock::now();
		const auto jitterUs = std::chrono::duration_cast<std::chrono::microseconds>(actualTime - waKeUpTime).count();
		if (jitterUs > 10 || jitterUs < -10) {
			std::cerr << "Warning: PPS jitter exceeded 10us: " << jitterUs << "us" << std::endl;
			continue; // skip this pulse if jitter is too high
		}else
		{
			std::cout << "PPS pulse generated with jitter: " << jitterUs << "us" << std::endl;
			for(auto& syncOut : syncOutsLines)
			{
				gpiod_line_set_value(syncOut, 1);
			}

			jitter.update(jitterUs);
			jitter.report(jitterUs);

			const uint64_t secs = nextMillisFromEpoch / 1000;
			NMEA::timestamp ts = NMEA::GetTimestampFromSec(secs);


			auto t = std::thread([&serialPorts, ts]() {
					std::this_thread::sleep_for(std::chrono::milliseconds(5));
					const std::string nmeaMessage = NMEA::produceNMEA(ts);
				for(auto& serialPort : serialPorts)
				{
					serialPort->Write(nmeaMessage);
				}
			});

			std::this_thread::sleep_for(std::chrono::milliseconds(50));

			for(auto& syncOut : syncOutsLines)
			{
				gpiod_line_set_value(syncOut, 0);
			}





			t.join();

		}
		
	}
	for(auto& syncOut : syncOutsLines)
	{
		gpiod_line_release(syncOut);
	}
	gpiod_chip_close(chip);
}
int main(int arc, char* argv[])
{
	    // Set realtime scheduling
    struct sched_param param;
    param.sched_priority = 99;  // highest priority
    if(sched_setscheduler(0, SCHED_FIFO, &param) < 0)
    {
        std::cerr << "Failed to set realtime priority, run as root" << std::endl;
    }

    // Lock memory to prevent page faults
    if(mlockall(MCL_CURRENT | MCL_FUTURE) < 0)
    {
        std::cerr << "Failed to lock memory" << std::endl;
    }


	std::cout << "fake pps" << std::endl;

	std::thread t1(oneSecondThread);
	while(!stop)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
	t1.join();
	return 0;
}
