
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
#include "gnss.h"
#include "publisher.h"
#include "compilation_constants.h"
#include <chrono>

#define MANDEYE_LIVOX_LISTEN_IP "192.168.1.5"
#define MANDEYE_REPO "/media/usb/"
#define MANDEYE_GPIO_SIM false
#define SERVER_PORT 8003

namespace utils
{
std::string getEnvString(const std::string& env, const std::string& def);
bool getEnvBool(const std::string& env, bool def);
} // namespace utils

namespace mandeye
{
enum class States
{
	WAIT_FOR_RESOURCES = -10,
	IDLE = 0,
	STARTING_SCAN = 10,
	SCANNING = 20,
	STOPPING = 30,
	STOPPING_STAGE_1 = 31,
	STOPPING_STAGE_2 = 32,
	STOPPED = 40,
	STARTING_STOP_SCAN = 100,
	STOP_SCAN_IN_PROGRESS = 150,
	STOP_SCAN_IN_INITIAL_PROGRESS = 160,
	STOPING_STOP_SCAN = 190,
	LIDAR_ERROR = 200,
	USB_IO_ERROR = 210,
};

const std::map<States, std::string> StatesToString{
	{States::WAIT_FOR_RESOURCES, "WAIT_FOR_RESOURCES"},
	{States::IDLE, "IDLE"},
	{States::STARTING_SCAN, "STARTING_SCAN"},
	{States::SCANNING, "SCANNING"},
	{States::STOPPING, "STOPPING"},
	{States::STOPPING_STAGE_1, "STOPPING_STAGE_1"},
	{States::STOPPING_STAGE_2, "STOPPING_STAGE_2"},
	{States::STOPPED, "STOPPED"},
	{States::STARTING_STOP_SCAN, "STARTING_STOP_SCAN"},
	{States::STOP_SCAN_IN_PROGRESS, "STOP_SCAN_IN_PROGRESS"},
	{States::STOP_SCAN_IN_INITIAL_PROGRESS, "STOP_SCAN_IN_INITIAL_PROGRESS"},
	{States::STOPING_STOP_SCAN, "STOPING_STOP_SCAN"},
	{States::LIDAR_ERROR, "LIDAR_ERROR"},
	{States::USB_IO_ERROR, "USB_IO_ERROR"},
};

std::atomic<bool> isRunning{true};
std::atomic<bool> isLidarError{false};
std::mutex livoxClientPtrLock;
std::shared_ptr<LivoxClient> livoxCLientPtr;
std::shared_ptr<GNSSClient> gnssClientPtr;
std::mutex gpioClientPtrLock;
std::shared_ptr<GpioClient> gpioClientPtr;
std::shared_ptr<FileSystemClient> fileSystemClientPtr;
std::shared_ptr<Publisher> publisherPtr;
mandeye::LazStats lastFileSaveStats;
double usbWriteSpeed10Mb = 0.0;
double usbWriteSpeed1Mb = 0.0;

bool disableBuzzer = false;
mandeye::States app_state{mandeye::States::WAIT_FOR_RESOURCES};

using json = nlohmann::json;

std::string produceReport(bool reportUSB = true)
{
	json j;
	j["name"] = "Mandeye";
	j["hash"] = GIT_HASH;
	j["version"] = MANDEYE_VERSION;
	j["hardware"] = MANDEYE_HARDWARE_HEADER;
	j["arch"] = SYSTEM_ARCH;
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
	j["fs_benchmark"]["write_speed_10mb"] = std::round(usbWriteSpeed10Mb * 100) / 100.0;
	j["fs_benchmark"]["write_speed_1mb"] = std::round(usbWriteSpeed1Mb * 100) / 100.0;

	if(fileSystemClientPtr && reportUSB)
	{
		j["fs"] = fileSystemClientPtr->produceStatus();
	}
	if(gnssClientPtr)
	{
		j["gnss"] = gnssClientPtr->produceStatus();
	}else{
		j["gnss"] = {};
	}

	j["lastLazStatus"] = lastFileSaveStats.produceStatus();

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

using namespace std::chrono_literals;

bool TriggerStopScan()
{
	if(app_state == States::IDLE || app_state == States::STOPPED)
	{
		app_state = States::STARTING_STOP_SCAN;
		return true;
	}
	return false;
}


std::chrono::steady_clock::time_point stoppingStage1StartDeadline;
std::chrono::steady_clock::time_point stoppingStage1StartDeadlineChangeLed;
std::chrono::steady_clock::time_point stoppingStage2StartDeadline;
std::chrono::steady_clock::time_point stoppingStage2StartDeadlineChangeLed;

bool TriggerContinousScanning(){
	if(app_state == States::IDLE || app_state == States::STOPPED){

		// intiliaze duration count
		if (livoxCLientPtr) {
			livoxCLientPtr->initializeDuration();
		}

		app_state = States::STARTING_SCAN;
		return true;
	}else if(app_state == States::SCANNING)
	{
#ifdef MANDEYE_COUNTINOUS_SCANNING_STOP_1_CLICK
		app_state = States::STOPPING;
		return true;
#endif //MANDEYE_COUNTINOUS_SCANNING_STOP_1_CLICK
		app_state = States::STOPPING_STAGE_1;
		//stoppingStage1Start = std::chrono::steady_clock::now();
		stoppingStage1StartDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);

		stoppingStage1StartDeadlineChangeLed = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);

		return true;
	}else if(app_state == States::STOPPING_STAGE_1){

		const auto now = std::chrono::steady_clock::now();

		if(now < stoppingStage1StartDeadline){
			//stoppingStage2Start = std::chrono::steady_clock::now();
			stoppingStage2StartDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);

			stoppingStage2StartDeadlineChangeLed = std::chrono::steady_clock::now() + std::chrono::milliseconds(25);

			app_state = States::STOPPING_STAGE_2;

		}

		return true;
	}else if(app_state == States::STOPPING_STAGE_2){
		const auto now = std::chrono::steady_clock::now();
		if(now < stoppingStage2StartDeadline){
			app_state = States::STOPPING;
		}
		return true;
	}
	return false;
}

void savePointcloudData(LivoxPointsBufferPtr buffer, const std::string& directory, int chunk)
{
	using namespace std::chrono_literals;
	char lidarName[256];

	const auto start = std::chrono::steady_clock::now();
	snprintf(lidarName, 256, "lidar%04d.laz", chunk);
	std::filesystem::path lidarFilePath = std::filesystem::path(directory) / std::filesystem::path(lidarName);
	std::cout << "Savig lidar buffer of size " << buffer->size() << " to " << lidarFilePath << std::endl;
	auto saveStatus = saveLaz(lidarFilePath.string(), buffer);

	system("sync");
	const auto end = std::chrono::steady_clock::now();
	const std::chrono::duration<float> elapsed_seconds = end - start;
	if (saveStatus) {
		saveStatus->m_saveDurationSec2 = elapsed_seconds.count();
		mandeye::lastFileSaveStats = *saveStatus;
	}
	return;
}

void saveLidarList(const std::unordered_map<uint32_t, std::string> &lidars, const std::string& directory, int chunk)
{
	using namespace std::chrono_literals;
	char lidarName[256];
	snprintf(lidarName, 256, "lidar%04d.sn", chunk);
	std::filesystem::path lidarFilePath = std::filesystem::path(directory) / std::filesystem::path(lidarName);
	std::cout << "Savig lidar list of size " << lidars.size() << " to " << lidarFilePath << std::endl;

	std::ofstream lidarStream(lidarFilePath);
	for (const auto &[id,sn] : lidars){
		lidarStream << id << " " << sn << "\n";
	}
	system("sync");
	return;
}

void saveStatusData(const std::string& directory, int chunk)
{
	using namespace std::chrono_literals;
	char statusName[256];
	snprintf(statusName, 256, "status%04d.json", chunk);
	std::filesystem::path lidarFilePath = std::filesystem::path(directory) / std::filesystem::path(statusName);
	std::cout << "Savig status to " << lidarFilePath << std::endl;
	std::ofstream lidarStream(lidarFilePath);
	lidarStream << produceReport(false);
	system("sync");
}

void saveImuData(LivoxIMUBufferPtr buffer, const std::string& directory, int chunk)
{
	using namespace std::chrono_literals;
	char lidarName[256];
	snprintf(lidarName, 256, "imu%04d.csv", chunk);
	std::filesystem::path lidarFilePath = std::filesystem::path(directory) / std::filesystem::path(lidarName);
	std::cout << "Savig imu buffer of size " << buffer->size() << " to " << lidarFilePath << std::endl;
	std::ofstream lidarStream(lidarFilePath.c_str());
	lidarStream << "timestamp gyroX gyroY gyroZ accX accY accZ imuId timestampUnix\n";
	std::stringstream ss;

	for(const auto& p : *buffer)
	{
		if(p.timestamp > 0){
			ss << p.timestamp << " " << p.point.gyro_x << " " << p.point.gyro_y << " " << p.point.gyro_z << " " << p.point.acc_x << " "
					<< p.point.acc_y << " " << p.point.acc_z << " " << p.laser_id  << " " <<p.epoch_time << "\n";
		}
	}
	lidarStream << ss.rdbuf();
	
	lidarStream.close();
	system("sync");
	return;
}

void saveGnssData(std::deque<std::string>& buffer, const std::string& directory, int chunk)
{
	using namespace std::chrono_literals;
	char lidarName[256];
	snprintf(lidarName, 256, "gnss%04d.gnss", chunk);
	std::filesystem::path lidarFilePath = std::filesystem::path(directory) / std::filesystem::path(lidarName);
	std::cout << "Savig gnss buffer of size " << buffer.size() << " to " << lidarFilePath << std::endl;
	std::ofstream lidarStream(lidarFilePath.c_str());
	std::stringstream ss;

	for(const auto& p : buffer)
	{
		ss << p ;
	}
	lidarStream << ss.rdbuf();

	lidarStream.close();
	system("sync");
	return;
}

void saveGnssRawData(std::deque<std::string>& buffer, const std::string& directory, int chunk)
{
	using namespace std::chrono_literals;
	char lidarName[256];
	snprintf(lidarName, 256, "gnss%04d.nmea", chunk);
	std::filesystem::path lidarFilePath = std::filesystem::path(directory) / std::filesystem::path(lidarName);
	std::cout << "Savig gnss raw buffer of size " << buffer.size() << " to " << lidarFilePath << std::endl;
	std::ofstream lidarStream(lidarFilePath.c_str());
	std::stringstream ss;

	for(const auto& p : buffer)
	{
		ss << p ;
	}
	lidarStream << ss.rdbuf();

	lidarStream.close();
	system("sync");
	return;
}


void stateWatcher()
{
	using namespace std::chrono_literals;
	std::chrono::steady_clock::time_point chunkStart = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point stopScanDeadline = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point stopScanInitialDeadline = std::chrono::steady_clock::now();
	States oldState = States::IDLE;
	std::string continousScanDirectory;
	std::string stopScanDirectory;
	int chunksInExperimentCS{0};
	int chunksInExperimentSS{0};

	int id_manifest = 0;
	if (stopScanDirectory.empty() && fileSystemClientPtr)
	{
		if(!fileSystemClientPtr->CreateDirectoryForStopScans(stopScanDirectory, id_manifest)){
			app_state = States::USB_IO_ERROR;
		}
	}
	if(stopScanDirectory.empty()){
		app_state = States::USB_IO_ERROR;
	}

	if(!fileSystemClientPtr->CreateDirectoryForContinousScanning(continousScanDirectory, id_manifest)){
		app_state = States::USB_IO_ERROR;
	}
	if(fileSystemClientPtr){
#ifdef MANDEYE_BENCHMARK_WRITE_SPEED
		std::cout << "Benchmarking write speed" << std::endl;
		mandeye::usbWriteSpeed10Mb=fileSystemClientPtr->BenchmarkWriteSpeed("benchmark10.bin", 10);
		mandeye::usbWriteSpeed1Mb=fileSystemClientPtr->BenchmarkWriteSpeed("benchmark1.bin", 1);
		std::cout << "Benchmarking write speed done" << std::endl;
#endif

	}

	while(isRunning)
	{
		if (mandeye::publisherPtr){
			mandeye::publisherPtr->SetMode(StatesToString.at(app_state));

		}
		if(oldState != app_state)
		{
			std::cout << "State transtion from " << StatesToString.at(oldState) << " to " << StatesToString.at(app_state) << std::endl;
		}
		oldState = app_state;
		
		if(app_state == States::LIDAR_ERROR){
			if(mandeye::gpioClientPtr){
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_STOP_SCAN, false);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_COPY_DATA, false);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, false);
				std::this_thread::sleep_for(1000ms);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_STOP_SCAN, true);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_COPY_DATA, false);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, true);
				std::this_thread::sleep_for(1000ms);
				std::cout << "app_state == States::LIDAR_ERROR" << std::endl;
				if (!disableBuzzer)
				{
					mandeye::gpioClientPtr->beep({2000, 100, 2000, 100, 2000, 100, 10000});
				}
			}
		}if(app_state == States::USB_IO_ERROR){
			if(mandeye::gpioClientPtr){
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_STOP_SCAN, false);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_COPY_DATA, false);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, false);
				std::this_thread::sleep_for(1000ms);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_STOP_SCAN, false);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_COPY_DATA, true);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, false);
				std::this_thread::sleep_for(1000ms);
				std::cout << "app_state == States::USB_IO_ERROR" << std::endl;
				if(!disableBuzzer)
				{
					mandeye::gpioClientPtr->beep({2000, 100, 2000, 100, 2000,100, 5000});
				}
			}
		}
		else if(app_state == States::WAIT_FOR_RESOURCES)
		{
			std::this_thread::sleep_for(100ms);
			std::lock_guard<std::mutex> l1(livoxClientPtrLock);
			std::lock_guard<std::mutex> l2(gpioClientPtrLock);
			if(mandeye::gpioClientPtr && mandeye::fileSystemClientPtr)
			{
				app_state = States::IDLE;
				// play hello beep
				if (!disableBuzzer)
				{
					mandeye::gpioClientPtr->beep({100, 50, 100, 50, 100, 50, 100, 50, 100, 50, 100, 50}); // beep, beep, beeeeeep
				}

			}
			if (mandeye::publisherPtr)
			{
				mandeye::publisherPtr->SetWorkingDirectory(stopScanDirectory, continousScanDirectory);
			}
		}
		else if(app_state == States::IDLE)
		{
			if(isLidarError)
			{
				app_state = States::LIDAR_ERROR;
			}
			if(gpioClientPtr)
			{
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_STOP_SCAN, false);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_COPY_DATA, false);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, false);
			}
			std::this_thread::sleep_for(100ms);
		}
		else if(app_state == States::STARTING_SCAN)
		{
			//chunksInExperiment = 0;
			if(gpioClientPtr)
			{
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_STOP_SCAN, false);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_COPY_DATA, false);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, true);
				std::this_thread::sleep_for(100ms);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, false);
				std::this_thread::sleep_for(100ms);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, true);
				std::this_thread::sleep_for(100ms);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, false);
				std::this_thread::sleep_for(100ms);
			}

			if(livoxCLientPtr)
			{
				livoxCLientPtr->startLog();
				if (gnssClientPtr)
				{
					gnssClientPtr->startLog();
				}
				app_state = States::SCANNING;
			}
			// create directory
			//if(!fileSystemClientPtr->CreateDirectoryForExperiment(continousScanDirectory)){
			//	app_state = States::USB_IO_ERROR;
			//}
			chunkStart = std::chrono::steady_clock::now();
		}
		else if(app_state == States::SCANNING || app_state == States::STOPPING_STAGE_1 || app_state == States::STOPPING_STAGE_2)
		{
			if(gpioClientPtr)
			{
				if(app_state == States::SCANNING){
					mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, true);
				}
			}
			
			const auto now = std::chrono::steady_clock::now();
			if(now - chunkStart > std::chrono::seconds(60))
			{
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, false);
				std::this_thread::sleep_for(1000ms);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, true);
				std::this_thread::sleep_for(1000ms);
			}
			if(now - chunkStart > std::chrono::seconds(600))
			{
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, false);
				std::this_thread::sleep_for(100ms);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, true);
				std::this_thread::sleep_for(100ms);
			}
			if(now - chunkStart > std::chrono::seconds(10) && app_state == States::SCANNING)
			{

				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_COPY_DATA, true);
				

				chunkStart = std::chrono::steady_clock::now();

				auto [lidarBuffer, imuBuffer] = livoxCLientPtr->retrieveData();
				std::deque<std::string> gnssBuffer;
				std::deque<std::string> gnssRawBuffer;

				if (gnssClientPtr)
				{
					gnssBuffer = gnssClientPtr->retrieveData();
					gnssRawBuffer = gnssClientPtr->retrieveRawData();
				}
				if(continousScanDirectory == ""){
					app_state = States::USB_IO_ERROR;
				}else{
					savePointcloudData(lidarBuffer, continousScanDirectory, chunksInExperimentCS + chunksInExperimentSS);
					saveImuData(imuBuffer, continousScanDirectory, chunksInExperimentCS + chunksInExperimentSS);
					saveStatusData(continousScanDirectory, chunksInExperimentCS + chunksInExperimentSS);
					auto lidarList = livoxCLientPtr->getSerialNumberToLidarIdMapping();
					saveLidarList(lidarList, continousScanDirectory, chunksInExperimentCS + chunksInExperimentSS);
					if (gnssClientPtr)
					{
						saveGnssData(gnssBuffer, continousScanDirectory, chunksInExperimentCS + chunksInExperimentSS);
						saveGnssRawData(gnssRawBuffer, continousScanDirectory, chunksInExperimentCS + chunksInExperimentSS);
					}
					mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_COPY_DATA, false);
					chunksInExperimentCS++;
				}
			}


			if(app_state == States::STOPPING_STAGE_1){
				std::cout << "app_state == States::STOPPING_STAGE_1" << std::endl;

				const auto now = std::chrono::steady_clock::now();

				if(now > stoppingStage1StartDeadlineChangeLed){
					static bool led_1 = true;

					mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, led_1);

					led_1 = !led_1;

					stoppingStage1StartDeadlineChangeLed = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
				}

				if(now > stoppingStage1StartDeadline){
					app_state = States::SCANNING;
				}
			}

			if(app_state == States::STOPPING_STAGE_2){
				std::cout << "app_state == States::STOPPING_STAGE_2" << std::endl;

				const auto now = std::chrono::steady_clock::now();

				if(now > stoppingStage2StartDeadlineChangeLed){
					static bool led_2 = true;

					mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, led_2);

					led_2 = !led_2;

					stoppingStage2StartDeadlineChangeLed = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
				}

				if(now > stoppingStage2StartDeadline){
					app_state = States::SCANNING;
				}
			}


			std::this_thread::sleep_for(20ms);
		}
		else if(app_state == States::STOPPING)
		{
			mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, true);
			mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_COPY_DATA, true);

			auto [lidarBuffer, imuBuffer] = livoxCLientPtr->retrieveData();
			std::deque<std::string> gnssData;
			livoxCLientPtr->stopLog();
			if (gnssClientPtr)
			{
				gnssData = gnssClientPtr->retrieveData();
				gnssClientPtr->stopLog();
			}

			for(int i = 0 ; i < 20; i++){
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, true);
				std::this_thread::sleep_for(50ms);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, false);
				std::this_thread::sleep_for(50ms);
			}

			mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, true);
			mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_COPY_DATA, true);

			if(continousScanDirectory.empty()){
				app_state = States::USB_IO_ERROR;
			}else{
				savePointcloudData(lidarBuffer, continousScanDirectory, chunksInExperimentCS + chunksInExperimentSS);
				saveImuData(imuBuffer, continousScanDirectory, chunksInExperimentCS + chunksInExperimentSS);
				saveStatusData(continousScanDirectory, chunksInExperimentCS + chunksInExperimentSS);
				auto lidarList = livoxCLientPtr->getSerialNumberToLidarIdMapping();
				saveLidarList(lidarList, continousScanDirectory, chunksInExperimentCS + chunksInExperimentSS);
				if (gnssClientPtr)
				{
					saveGnssData(gnssData, continousScanDirectory, chunksInExperimentCS + chunksInExperimentSS);
				}
				chunksInExperimentCS++;
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_COPY_DATA, false);
				app_state = States::IDLE;
			}
		}
		else if(app_state == States::STARTING_STOP_SCAN)
		{
			if(gpioClientPtr)
			{
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_STOP_SCAN, false);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_COPY_DATA, false);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, false);
			}
			//if (stopScanDirectory.empty() && fileSystemClientPtr)
			//{
			//	if(!fileSystemClientPtr->CreateDirectoryForStopScans(stopScanDirectory)){
			//		app_state = States::USB_IO_ERROR;
			//	}
			//}
			//if(stopScanDirectory.empty()){
			//	app_state = States::USB_IO_ERROR;
			//}

			stopScanInitialDeadline = std::chrono::steady_clock::now();
			stopScanInitialDeadline += std::chrono::milliseconds(5000);

			stopScanDeadline = stopScanInitialDeadline + std::chrono::milliseconds(30000);

			app_state = States::STOP_SCAN_IN_INITIAL_PROGRESS;
		}else if(app_state == States::STOP_SCAN_IN_INITIAL_PROGRESS){
			const auto now = std::chrono::steady_clock::now();

			if(now < stopScanInitialDeadline){
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_STOP_SCAN, false);
				std::this_thread::sleep_for(100ms);
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_STOP_SCAN, true);
				std::this_thread::sleep_for(100ms);
			}else{
				if(livoxCLientPtr)
				{
					mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_STOP_SCAN, true);
					livoxCLientPtr->startLog();
				}
				if (gnssClientPtr)
				{
					gnssClientPtr->startLog();
				}
				app_state = States::STOP_SCAN_IN_PROGRESS;
			}
		}
		else if(app_state == States::STOP_SCAN_IN_PROGRESS)
		{
			const auto now = std::chrono::steady_clock::now();
			if(now > stopScanDeadline){
				app_state = States::STOPING_STOP_SCAN;
			}
		}
		else if(app_state == States::STOPING_STOP_SCAN)
		{
			if(gpioClientPtr)
			{
				mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_COPY_DATA, true);
			}
			auto [lidarBuffer, imuBuffer] = livoxCLientPtr->retrieveData();
			std::deque<std::string> gnssData;
			livoxCLientPtr->stopLog();
			if (gnssClientPtr)
			{
				gnssData = gnssClientPtr->retrieveData();
				gnssClientPtr->stopLog();
			}
			if(stopScanDirectory.empty()){
				app_state = States::USB_IO_ERROR;
			}else{
				savePointcloudData(lidarBuffer, stopScanDirectory, chunksInExperimentCS + chunksInExperimentSS);
				saveImuData(imuBuffer, stopScanDirectory, chunksInExperimentCS + chunksInExperimentSS);
				saveStatusData(stopScanDirectory, chunksInExperimentCS + chunksInExperimentSS);
				auto lidarList = livoxCLientPtr->getSerialNumberToLidarIdMapping();
				saveLidarList(lidarList, stopScanDirectory, chunksInExperimentCS + chunksInExperimentSS);
				if (gnssClientPtr)
				{
					saveGnssData(gnssData, stopScanDirectory, chunksInExperimentCS + chunksInExperimentSS);
				}
				chunksInExperimentSS++;
				
				if(gpioClientPtr)
				{
					mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_COPY_DATA, false);
					mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_STOP_SCAN, false);
				}
				app_state = States::IDLE;
			}
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


#ifdef PISTACHE_SERVER
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
			std::string p = mandeye::produceReport(false);
			writer.send(Http::Code::Ok, p);
			return;
		}
		if(request.resource() == "/status_full" || request.resource() == "/json/status_full")
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
#endif


int main(int argc, char** argv)
{
	std::cout << "program: " << argv[0] << " "<<MANDEYE_VERSION <<" " << MANDEYE_HARDWARE_HEADER << std::endl;
	Address addr(Ipv4::any(), SERVER_PORT);

	mandeye::disableBuzzer = utils::getEnvBool("MANDEYE_DISABLE_BUZZER", false);

	std::cout << "Buzzer is " << (mandeye::disableBuzzer ? "disabled" : "enabled") << std::endl;
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
		if(!mandeye::livoxCLientPtr->startListener(utils::getEnvString("MANDEYE_LIVOX_LISTEN_IP", MANDEYE_LIVOX_LISTEN_IP))){
			mandeye::isLidarError.store(true);
		}

		// intialize in this thread to prevent initialization fiasco
        const std::string portName = hardware::GetGNSSPort();
		const auto baud = hardware::GetGNSSBaudrate();
        if (!portName.empty())
        {
            mandeye::gnssClientPtr = std::make_shared<mandeye::GNSSClient>();
            mandeye::gnssClientPtr->SetTimeStampProvider(mandeye::livoxCLientPtr);
            mandeye::gnssClientPtr->startListener(portName, baud);

			// set callback
			mandeye::gnssClientPtr->setDataCallback( [&](const minmea_sentence_gga& gga)
			{
				if(mandeye::gpioClientPtr && gga.fix_quality > 0 && gga.satellites_tracked > 5 && !mandeye::disableBuzzer ) // if any fix quality is available
				{
					std::lock_guard<std::mutex> l2(mandeye::gpioClientPtrLock);
					mandeye::gpioClientPtr->setLed(hardware::LED::BUZZER, true);
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
					mandeye::gpioClientPtr->setLed(hardware::LED::BUZZER, false);
				}
			});

        }
		// start zeromq publisher
		mandeye::publisherPtr = std::make_shared<mandeye::Publisher>();
		mandeye::publisherPtr->SetTimeStampProvider(mandeye::livoxCLientPtr);

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
			mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_STOP_SCAN, true);
			std::this_thread::sleep_for(100ms);
			mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_STOP_SCAN, false);
			std::this_thread::sleep_for(100ms);

			mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, true);
			std::this_thread::sleep_for(100ms);
			mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_CONTINOUS_SCANNING, false);
			std::this_thread::sleep_for(100ms);

			mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_COPY_DATA, true);
			std::this_thread::sleep_for(100ms);
			mandeye::gpioClientPtr->setLed(hardware::LED::LED_GPIO_COPY_DATA, false);
			std::this_thread::sleep_for(100ms);
		}
		std::cout << "GPIO Init done" << std::endl;
		mandeye::gpioClientPtr->addButtonCallback(hardware::BUTTON::BUTTON_STOP_SCAN, "BUTTON_STOP_SCAN", [&]() { mandeye::TriggerStopScan(); });
		mandeye::gpioClientPtr->addButtonCallback(hardware::BUTTON::BUTTON_CONTINOUS_SCANNING, "BUTTON_CONTINOUS_SCANNING", [&]() { mandeye::TriggerContinousScanning(); });
	});

	while(mandeye::isRunning)
	{
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(1000ms);
		char ch = std::getchar();
		if(ch == 'q')
		{
			mandeye::isRunning.store(false);
		}
		std::cout << "Press q -> quit, s -> start scan , e -> end scan" << std::endl;

		if(ch == 's')
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
