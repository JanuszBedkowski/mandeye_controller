#pragma once

#include <deque>
#include <filesystem>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <fstream>
namespace mandeye
{

class FileSystemClient
{
	constexpr static char manifestFilename[]{"mandala_manifest.txt"};
	constexpr static char config[]{"mandeye_config.json"};
	constexpr static char versionFilename[]{"version.txt"};
	constexpr static char buzzerTimestamps[]{"buzzer_timestamps.csv"};

public:
	FileSystemClient(const std::string& repository);
	nlohmann::json produceStatus();

	//! Test is writable
	float CheckAvailableSpace();

	//! Create Counter file
	int32_t GetIdFromManifest();

	//! Create Counter file
	int32_t GetNextIdFromManifest();

	//! Get is writable
	bool GetIsWritable();

	std::vector<std::string> GetDirectories();

	bool CreateDirectoryForContinousScanning(std::string&, const int&);

	bool CreateDirectoryForStopScans(std::string&, int& id_manifest);

	double BenchmarkWriteSpeed(const std::string& filename, size_t fileSizeMB);

	//! append a buzzer-on event to the buzzer csv of the current continous scanning directory
	bool LogBuzzer(uint64_t timestampNs, uint32_t durationMs);

	nlohmann::json GetConfig();

private:
	int32_t m_nextId{0};
	std::string ConvertToText(float mb);

	//! create "<dirPrefix>_<id>" in the repository; on failure the reason is left in m_error.
	//! note: takes m_mutex, so do not call it with the lock already held
	bool CreateScanDirectory(const char* dirPrefix, int id, std::filesystem::path& createdDir);

	std::string m_repository;
	std::string m_currentContinousScanDirectory;
	std::string m_currentStopScanDirectory;
	std::filesystem::path m_logBuzzerFilename;
	std::ofstream m_logBuzzer;

	std::string m_error;
	std::mutex m_mutex;
	double m_benchmarkWriteSpeed{-1.f};
};
} // namespace mandeye