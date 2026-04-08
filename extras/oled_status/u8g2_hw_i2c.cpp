#include <U8g2lib.h>

// Set I2C bus and address
#define I2C_BUS 1
#define I2C_ADDRESS 0x3c * 2
// Check https://github.com/olikraus/u8g2/wiki/u8g2setupcpp for all supported devices
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/
												U8X8_PIN_NONE);

#include "../utils/ExtrasUtils.h"
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace state
{
std::mutex stateMutex;
std::string modeName;
uint64_t timestamp;
std::string continuousScanTarget;
} // namespace state

struct FileGroupStats
{
	std::string label;
	int count = 0;
	double megabytes = 0.0;
};

struct ScanStats
{
	FileGroupStats lidar; // .laz in root
	FileGroupStats imu; // .csv in root
	std::vector<FileGroupStats> cameras; // one per CAMERA* dir
	std::vector<FileGroupStats> leptons; // one per LEPTON* dir, name trimmed
	std::vector<FileGroupStats> extra_gnss; // one per LEPTON* dir, name trimmed
};

static void accumulateDir(const fs::path& dir, FileGroupStats& g)
{
	for(const auto& e : fs::recursive_directory_iterator(dir))
	{
		if(e.is_regular_file())
		{
			g.count++;
			g.megabytes += static_cast<double>(e.file_size()) / (1024.0 * 1024.0);
		}
	}
}

// Keep prefix + up to maxSuffix chars of the remainder
static std::string trimDirName(const std::string& name, const std::string& prefix, size_t maxSuffix)
{
	if(name.size() <= prefix.size())
		return name;
	const std::string suffix = name.substr(prefix.size());
	return prefix + suffix.substr(0, maxSuffix);
}

ScanStats scanDirectory(const std::string& path)
{
	ScanStats stats;
	stats.lidar.label = "LAZ";
	stats.imu.label = "CSV";
	if(path.empty() || !fs::is_directory(path))
		return stats;

	for(const auto& entry : fs::directory_iterator(path))
	{
		if(entry.is_regular_file())
		{
			const auto ext = entry.path().extension().string();
			const auto size_mb = static_cast<double>(entry.file_size()) / (1024.0 * 1024.0);
			if(ext == ".laz")
			{
				stats.lidar.count++;
				stats.lidar.megabytes += size_mb;
			}
			else if(ext == ".csv")
			{
				stats.imu.count++;
				stats.imu.megabytes += size_mb;
			}
		}
		else if(entry.is_directory())
		{
			const auto dirname = entry.path().filename().string();
			if(dirname.rfind("CAMERA", 0) == 0)
			{
				FileGroupStats g;
				// CAMERA_1 -> CAM_1 (keep "CAM" + suffix after "CAMERA")
				g.label = "CAM" + dirname.substr(6);
				accumulateDir(entry.path(), g);
				stats.cameras.push_back(std::move(g));
			}
			else if(dirname.rfind("LEPTON", 0) == 0)
			{
				FileGroupStats g;
				// LEPTON_longname -> LEP_ + first 4 chars of suffix
				g.label = "LEP" + trimDirName(dirname.substr(6), "", 5);
				accumulateDir(entry.path(), g);
				stats.leptons.push_back(std::move(g));
			}
		}
	}

	std::sort(stats.cameras.begin(), stats.cameras.end(), [](const FileGroupStats& a, const FileGroupStats& b) { return a.label < b.label; });
	std::sort(stats.leptons.begin(), stats.leptons.end(), [](const FileGroupStats& a, const FileGroupStats& b) { return a.label < b.label; });

	return stats;
}

void clientThread()
{
	mandeye::extras::startZeroMQListener([](const nlohmann::json& j) {
		std::lock_guard<std::mutex> lck(state::stateMutex);
		if(j.contains(mandeye::extras::keys::TIME))
		{
			state::timestamp = j[mandeye::extras::keys::TIME].get<uint64_t>();
		}
		if(j.contains(mandeye::extras::keys::MODE))
		{
			state::modeName = j[mandeye::extras::keys::MODE].get<std::string>();
		}
		if(j.contains(mandeye::extras::keys::CONTINUOUS_SCAN_DIRECTORY))
		{
			state::continuousScanTarget = j[mandeye::extras::keys::CONTINUOUS_SCAN_DIRECTORY].get<std::string>();
		}
	});
}

int main(void)
{
	std::thread zmqThread(clientThread);
	// GPIO chip doesn't matter for hardware I2C
	u8g2.initI2cHw(I2C_BUS);
	u8g2.setI2CAddress(I2C_ADDRESS);
	u8g2.begin();
	u8g2.setFont(u8g2_font_6x10_tr);

	constexpr int LINE_H = 11;
	constexpr int Y0 = LINE_H;
	constexpr int Y1 = LINE_H * 2;
	constexpr int Y2 = LINE_H * 3;
	constexpr int Y3 = LINE_H * 4;

	for(;;)
	{
		std::string scanPath;
		{
			std::lock_guard<std::mutex> lck(state::stateMutex);
			scanPath = state::continuousScanTarget;
		}

		const ScanStats stats = scanDirectory(scanPath);

		u8g2.clearBuffer();

		// Line 1: mode + LAZ/CSV summary
		{
			std::lock_guard<std::mutex> lck(state::stateMutex);
			std::ostringstream ss;
			ss << state::modeName << " L:" << stats.lidar.count << " C:" << stats.imu.count;
			u8g2.drawStr(1, Y0, ss.str().c_str());
		}
		// line2 : timestamp
		{
			std::lock_guard<std::mutex> lck(state::stateMutex);
			std::ostringstream ss;
			ss << std::fixed << std::setprecision(1) << "T:" << state::timestamp / 1e9;
			u8g2.drawStr(1, Y1, ss.str().c_str());
		}
		// line3 cameras:
		{
			std::ostringstream ss;
			for(int i = 0; i < stats.cameras.size(); i++)
			{
				ss << "C" << i << ":" << stats.cameras[i].count << "MB ";
			}
			u8g2.drawStr(1, Y2, ss.str().c_str());
		}
		// line4 leptons:
		{
			std::ostringstream ss;
			for(int i = 0; i < stats.leptons.size(); i++)
			{
				ss << "L" << i << ":" << stats.leptons[i].count << "MB ";
			}
			u8g2.drawStr(1, Y3, ss.str().c_str());
		}

		u8g2.sendBuffer();
	}
	u8g2.doneI2c();
	u8g2.doneUserData();
	zmqThread.join();
}
