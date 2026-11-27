// Standalone UDP -> pcap recorder, no root needed. Writes <dir>/lidarNNNN.pcap, starting a new file every --rotate seconds.
//   pcap_udp_recorder /media/usb/pcap_test --port 2368 --rotate 5 --duration 60
// Exit code: 0 ok, 1 bad arguments, 2 could not open the port, 3 datagrams were lost (dropped or write errors).
#include "utils/PcapUdpRecorder.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace
{
std::atomic<bool> stopRequested{false};
void onSignal(int)
{
	stopRequested = true;
}
int usage()
{
	std::cerr << "usage: pcap_udp_recorder <output_dir> [--port 2368] [--rotate 5] [--duration 0 (until Ctrl-C)]" << std::endl;
	return 1;
}
} // namespace

int main(int argc, char** argv)
{
	if(argc < 2 || argc % 2 != 0)
	{
		return usage();
	}
	const std::filesystem::path directory = argv[1];
	int port = 2368;
	double rotateSeconds = 5.0;
	double durationSeconds = 0.0;
	try
	{
		for(int i = 2; i + 1 < argc; i += 2)
		{
			const std::string key = argv[i];
			const double value = std::stod(argv[i + 1]);
			if(key == "--port")
				port = static_cast<int>(value);
			else if(key == "--rotate")
				rotateSeconds = value;
			else if(key == "--duration")
				durationSeconds = value;
			else
				return usage();
		}
	}
	catch(const std::exception&)
	{
		return usage();
	}
	if(port <= 0 || port > 65535 || rotateSeconds <= 0.0)
	{
		return usage();
	}

	std::filesystem::create_directories(directory);
	std::signal(SIGINT, onSignal);
	std::signal(SIGTERM, onSignal);

	mandeye::PcapUdpRecorder recorder(static_cast<uint16_t>(port));
	if(!recorder.start())
	{
		return 2;
	}
	auto fileName = [&](int chunk) {
		char name[64];
		snprintf(name, sizeof(name), "lidar%04d.pcap", chunk);
		return (directory / name).string();
	};

	using Clock = std::chrono::steady_clock;
	const std::chrono::duration<double> rotate(rotateSeconds);
	const std::chrono::duration<double> duration(durationSeconds);
	int chunk = 0;
	recorder.startFile(fileName(chunk));
	const auto start = Clock::now();
	auto chunkStart = start;
	while(!stopRequested)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		const auto now = Clock::now();
		if(durationSeconds > 0.0 && now - start >= duration)
		{
			break;
		}
		if(now - chunkStart >= rotate)
		{
			chunkStart = now;
			recorder.startFile(fileName(++chunk));
			std::cout << recorder.produceStatus().dump() << std::endl;
		}
	}
	recorder.stop();

	const auto status = recorder.produceStatus();
	std::cout << status.dump(2) << std::endl;
	const uint64_t lost = status["packets_dropped_queue"].get<uint64_t>() + status["packets_dropped_kernel"].get<uint64_t>() +
						  status["packets_lost_write_error"].get<uint64_t>();
	return lost > 0 ? 3 : 0;
}
