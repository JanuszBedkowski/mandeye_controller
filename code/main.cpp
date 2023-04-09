
#include <chrono>
#include <json.hpp>
#include <ostream>
#include <thread>

#include "save_laz.h"
#include <FileSystemClient.h>
#include <LivoxClient.h>
#include <fstream>
#include <gpios.h>
#include <iostream>
#include <string>

//configuration for alienware
#define MANDEYE_LIVOX_LISTEN_IP "192.168.1.100"
#define MANDEYE_REPO "/tmp/"
#define MANDEYE_GPIO_SIM true

namespace mandeye
{
enum class States
{
	WAIT_FOR_RESOURCES = -10,
	IDLE = 0,
	STARTING_SCAN = 10,
	SCANNING = 20,
	STOPPING = 30,
	STOPPED = 40,
	STARTING_STOP_SCAN = 100,
	STOP_SCAN_IN_PROGRESS = 150,
	STOPING_STOP_SCAN = 190,
};

const std::map<States, std::string> StatesToString{
	{States::WAIT_FOR_RESOURCES, "WAIT_FOR_RESOURCES"},
	{States::IDLE, "IDLE"},
	{States::STARTING_SCAN, "STARTING_SCAN"},
	{States::SCANNING, "SCANNING"},
	{States::STOPPING, "STOPPING"},
	{States::STOPPED, "STOPPED"},
	{States::STARTING_STOP_SCAN, "STARTING_STOP_SCAN"},
	{States::STOP_SCAN_IN_PROGRESS, "STOP_SCAN_IN_PROGRESS"},
	{States::STOPING_STOP_SCAN, "STOPING_STOP_SCAN"},

};

std::atomic<bool> isRunning{true};
std::mutex livoxClientPtrLock;
std::shared_ptr<LivoxClient> livoxCLientPtr;
std::mutex gpioClientPtrLock;
std::shared_ptr<GpioClient> gpioClientPtr;
std::shared_ptr<FileSystemClient> fileSystemClientPtr;

mandeye::States app_state{mandeye::States::WAIT_FOR_RESOURCES};

using json = nlohmann::json;

std::string produceReport()
{
	json j;
	j["name"] = "Mandye";
	j["state"] = StatesToString.at(app_state);
	if(livoxCLientPtr)
	{
		j["livox"] = livoxCLientPtr->produceStatus();
	}
	else
	{
		j["livox"] = {};
	}

	if(gpioClientPtr)
	{
		j["gpio"] = gpioClientPtr->produceStatus();
	}

	if(fileSystemClientPtr)
	{
		j["fs"] = fileSystemClientPtr->produceStatus();
	}

	std::ostringstream s;
	s << std::setw(4) << j;
	return s.str();
}

bool StartScan()
{
	if(app_state == States::IDLE || app_state == States::STOPPED)
	{
		app_state = States::STARTING_SCAN;
		return true;
	}
	return false;
}
bool StopScan()
{
	if(app_state == States::SCANNING)
	{
		app_state = States::STOPPING;
		return true;
	}
	return false;
}

bool TriggerStopScan()
{
	if(app_state == States::IDLE || app_state == States::STOPPED)
	{
		app_state = States::STARTING_STOP_SCAN;
		return true;
	}
	return false;
}


void savePointcloudData(LivoxPointsBufferPtr buffer, const std::string& directory, int chunk)
{
	using namespace std::chrono_literals;
	char lidarName[256];
	snprintf(lidarName, 256, "lidar%04d.laz", chunk);
	std::filesystem::path lidarFilePath = std::filesystem::path(directory) / std::filesystem::path(lidarName);
	std::cout << "Savig lidar buffer of size " << buffer->size() << " to " << lidarFilePath << std::endl;
	saveLaz(lidarFilePath.string(), buffer);
	//        std::ofstream lidarStream(lidarFilePath.c_str());
	//        for (const auto &p : *buffer){
	//            lidarStream<<p.point.x << " "<<p.point.y <<"  "<<p.point.z << " "<< p.point.tag << " " << p.timestamp << "\n";
	//        }
}

void saveImuData(LivoxIMUBufferPtr buffer, const std::string& directory, int chunk)
{
	using namespace std::chrono_literals;
	char lidarName[256];
	snprintf(lidarName, 256, "imu%04d.csv", chunk);
	std::filesystem::path lidarFilePath = std::filesystem::path(directory) / std::filesystem::path(lidarName);
	std::cout << "Savig imu buffer of size " << buffer->size() << " to " << lidarFilePath << std::endl;
	std::ofstream lidarStream(lidarFilePath.c_str());
	for(const auto& p : *buffer)
	{
		lidarStream << p.timestamp << " " << p.point.gyro_x << " " << p.point.gyro_y << "  " << p.point.gyro_z << " " << p.point.acc_x << " "
					<< p.point.acc_y << "  " << p.point.acc_z << "\n";
	}
}
void stateWatcher()
{
	using namespace std::chrono_literals;
	std::chrono::steady_clock::time_point chunkStart = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point stopScanDeadline = std::chrono::steady_clock::now();
	States oldState = States::IDLE;
	std::string experimentDirectory;
	std::string stopScanDirectory;
	int chunksInExperiment{0};

	while(isRunning)
	{
		if(oldState != app_state)
		{
			std::cout << "State transtion from " << StatesToString.at(oldState) << " to " << StatesToString.at(app_state) << std::endl;
		}
		oldState = app_state;
		if(app_state == States::WAIT_FOR_RESOURCES)
		{
			std::this_thread::sleep_for(100ms);
			std::lock_guard<std::mutex> l1(livoxClientPtrLock);
			std::lock_guard<std::mutex> l2(gpioClientPtrLock);
			if(mandeye::gpioClientPtr && mandeye::gpioClientPtr && mandeye::fileSystemClientPtr)
			{
				app_state = States::IDLE;
			}
		}
		else if(app_state == States::IDLE)
		{
			if(gpioClientPtr)
			{
				mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, false);
				mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_RED, true);
				mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, false);
			}
			std::this_thread::sleep_for(100ms);
		}
		else if(app_state == States::STARTING_SCAN)
		{
			chunksInExperiment = 0;
			if(gpioClientPtr)
			{
				mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, false);
				mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_RED, false);
				mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, false);
			}

			if(livoxCLientPtr)
			{
				livoxCLientPtr->startLog();
				app_state = States::SCANNING;
			}
			// create directory
			experimentDirectory = fileSystemClientPtr->CreateDirectoryForExperiment();
			chunkStart = std::chrono::steady_clock::now();
		}
		else if(app_state == States::SCANNING)
		{
			if(gpioClientPtr)
			{
				mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, true);
			}
			const auto now = std::chrono::steady_clock::now();
			if(now - chunkStart > std::chrono::seconds(5))
			{
				mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, true);
				chunkStart = std::chrono::steady_clock::now();

				auto [lidarBuffer, imuBuffer] = livoxCLientPtr->retrieveData();
				savePointcloudData(lidarBuffer, experimentDirectory, chunksInExperiment);
				saveImuData(imuBuffer, experimentDirectory, chunksInExperiment);

				mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, false);
				chunksInExperiment++;
			}
			std::this_thread::sleep_for(100ms);
		}
		else if(app_state == States::STOPPING)
		{
			mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, true);

			auto [lidarBuffer, imuBuffer] = livoxCLientPtr->retrieveData();
			livoxCLientPtr->stopLog();

			savePointcloudData(lidarBuffer, experimentDirectory, chunksInExperiment);
			saveImuData(imuBuffer, experimentDirectory, chunksInExperiment);

			mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, false);
			app_state = States::IDLE;
		}
		else if(app_state == States::STARTING_STOP_SCAN)
		{
			if(gpioClientPtr)
			{
				mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, true);
				mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_RED, false);
				mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, false);
			}
			if (stopScanDirectory.empty() && fileSystemClientPtr)
			{
				stopScanDirectory = fileSystemClientPtr->CreateDirectoryForStopScans();
			}
			stopScanDeadline = std::chrono::steady_clock::now();
			stopScanDeadline += std::chrono::milliseconds(5000);
			if(livoxCLientPtr)
			{
				livoxCLientPtr->startLog();
				app_state = States::STOP_SCAN_IN_PROGRESS;
			}
		}
		else if(app_state == States::STOP_SCAN_IN_PROGRESS)
		{
			const auto now = std::chrono::steady_clock::now();
			if(now > stopScanDeadline)
			{
				if(gpioClientPtr)
				{
					mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, false);
					mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_RED, false);
					mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, true);
				}
				app_state = States::STOPING_STOP_SCAN;
			}
		}
		else if(app_state == States::STOPING_STOP_SCAN)
		{
			auto [lidarBuffer, imuBuffer] = livoxCLientPtr->retrieveData();
			livoxCLientPtr->stopLog();
			chunksInExperiment ++;
			savePointcloudData(lidarBuffer, stopScanDirectory, chunksInExperiment);
			saveImuData(imuBuffer, stopScanDirectory, chunksInExperiment);
			if(gpioClientPtr)
			{
				mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, false);
			}
			app_state = States::IDLE;
		}
	}
}
} // namespace mandeye

namespace utils
{
std::string getEnvString(const std::string& env, const std::string& def)
{
	const char* env_p = std::getenv(env.c_str());
	if(env_p == nullptr)
	{
		return def;
	}
	return std::string{env_p};
}

bool getEnvBool(const std::string& env, bool def)
{
	const char* env_p = std::getenv(env.c_str());
	if(env_p == nullptr)
	{
		return def;
	}
	if(strcmp("1", env_p) == 0 || strcmp("true", env_p) == 0)
	{
		return true;
	}
	return false;
}
} // namespace utils

#include "web_page.h"
#include <pistache/endpoint.h>
using namespace Pistache;

struct PistacheServerHandler : public Http::Handler
{
	HTTP_PROTOTYPE(PistacheServerHandler)
	void onRequest(const Http::Request& request, Http::ResponseWriter writer) override
	{
		if(request.resource() == "/status" || request.resource() == "/json/status")
		{
			std::string p = mandeye::produceReport();
			writer.send(Http::Code::Ok, p);
			return;
		}
		else if(request.resource() == "/jquery.js")
		{
			writer.send(Http::Code::Ok, gJQUERYData);
			return;
		}
		else if(request.resource() == "/trig/start_bag")
		{
			mandeye::StartScan();
			writer.send(Http::Code::Ok, "");
			return;
		}
		else if(request.resource() == "/trig/stop_bag")
		{
			mandeye::StopScan();
			writer.send(Http::Code::Ok, "");
			return;
		}
		else if(request.resource() == "/trig/stopscan")
		{
			mandeye::TriggerStopScan();
			writer.send(Http::Code::Ok, "");
			return;
		}
		writer.send(Http::Code::Ok, gINDEX_HTMData);
	}
};

int main(int argc, char** argv)
{
	Address addr(Ipv4::any(), 8003);

	auto server = std::make_shared<Http::Endpoint>(addr);
	std::thread http_thread1([&]() {
		auto opts = Http::Endpoint::options().threads(2);
		server->init(opts);
		server->setHandler(Http::make_handler<PistacheServerHandler>());
		server->serve();
	});

	mandeye::fileSystemClientPtr = std::make_shared<mandeye::FileSystemClient>(utils::getEnvString("MANDEYE_REPO", MANDEYE_REPO));

	std::thread thLivox([&]() {
		{
			std::lock_guard<std::mutex> l1(mandeye::livoxClientPtrLock);
			mandeye::livoxCLientPtr = std::make_shared<mandeye::LivoxClient>();
		}
		mandeye::livoxCLientPtr->startListener(utils::getEnvString("MANDEYE_LIVOX_LISTEN_IP", MANDEYE_LIVOX_LISTEN_IP));
	});

	std::thread thStateMachine([&]() { mandeye::stateWatcher(); });

	std::thread thGpio([&]() {
		std::lock_guard<std::mutex> l2(mandeye::gpioClientPtrLock);
		using namespace std::chrono_literals;
		const bool simMode = utils::getEnvBool("MANDEYE_GPIO_SIM", MANDEYE_GPIO_SIM);
		std::cout << "MANDEYE_GPIO_SIM : " << simMode << std::endl;
		mandeye::gpioClientPtr = std::make_shared<mandeye::GpioClient>(simMode);
		for(int i = 0; i < 3; i++)
		{
			mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, true);
			std::this_thread::sleep_for(100ms);
			mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_RED, true);
			std::this_thread::sleep_for(100ms);
			mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, true);
			std::this_thread::sleep_for(1000ms);
			mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_GREEN, false);
			std::this_thread::sleep_for(100ms);
			mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_RED, false);
			std::this_thread::sleep_for(100ms);
			mandeye::gpioClientPtr->setLed(mandeye::GpioClient::LED::LED_GPIO_YELLOW, false);
		}
		std::cout << "GPIO Init done" << std::endl;

		mandeye::gpioClientPtr->addButtonCallback(mandeye::GpioClient::BUTTON::BUTTON_1, "START_SCAN", [&]() { mandeye::StartScan(); });
		mandeye::gpioClientPtr->addButtonCallback(mandeye::GpioClient::BUTTON::BUTTON_2, "STOP_SCAN", [&]() { mandeye::StopScan(); });
	});

	while(mandeye::isRunning)
	{
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(1000ms);
		std::cout << "Press q -> quit, s -> start scan , e -> end scan" << std::endl;
		char ch = std::getchar();
		if(ch == 'q')
		{
			mandeye::isRunning.store(false);
		}
		else if(ch == 's')
		{
			if(mandeye::StartScan())
			{
				std::cout << "start scan success!" << std::endl;
			}
		}
		else if(ch == 'e')
		{
			if(mandeye::StopScan())
			{
				std::cout << "stop scan success!" << std::endl;
			}
		}
	}

	server->shutdown();
	http_thread1.join();
	std::cout << "joining thStateMachine" << std::endl;
	thStateMachine.join();

	std::cout << "joining thLivox" << std::endl;
	thLivox.join();

	std::cout << "joining thGpio" << std::endl;
	thGpio.join();
	std::cout << "Done" << std::endl;
	return 0;
}