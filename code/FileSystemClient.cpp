#include "FileSystemClient.h"
#include "compilation_constants.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdbool.h>
#include <unistd.h>
namespace mandeye
{

FileSystemClient::FileSystemClient(const std::string& repository)
	: m_repository(repository)
{
	m_nextId = GetIdFromManifest();
}
nlohmann::json FileSystemClient::produceStatus()
{
	nlohmann::json data;
	data["FileSystemClient"]["repository"] = m_repository;
	float free_mb = 0;

	try
	{
		free_mb = CheckAvailableSpace();
	}
	catch(std::filesystem::filesystem_error& e)
	{
		data["FileSystemClient"]["error"] = e.what();
	}
	data["FileSystemClient"]["buzzer_csv"] = m_logBuzzerFilename.c_str();
	data["FileSystemClient"]["free_megabytes"] = free_mb;
	data["FileSystemClient"]["free_str"] = ConvertToText(free_mb);
	data["FileSystemClient"]["benchmarkWriteSpeed"] = m_benchmarkWriteSpeed;
	try
	{
		data["FileSystemClient"]["m_nextId"] = m_nextId;
	}
	catch(std::filesystem::filesystem_error& e)
	{
		data["FileSystemClient"]["error"] = e.what();
	}
	try
	{
		data["FileSystemClient"]["writable"] = GetIsWritable();
	}
	catch(std::filesystem::filesystem_error& e)
	{
		data["FileSystemClient"]["error"] = e.what();
	}

	try
	{
		data["FileSystemClient"]["dirs"] = GetDirectories();
	}
	catch(std::filesystem::filesystem_error& e)
	{
		data["FileSystemClient"]["error"] = e.what();
	}
	return data;
}

//! Test is writable
float FileSystemClient::CheckAvailableSpace()
{
	std::error_code ec;
	const std::filesystem::space_info si = std::filesystem::space(m_repository, ec);
	if(ec.value() == 0)
	{
		const float f = static_cast<float>(si.free) / (1024 * 1024);
		return std::round(f);
	}
	return -1.f;
}

std::string FileSystemClient::ConvertToText(float mb)
{
	std::stringstream tmp;
	tmp << std::setprecision(1) << std::fixed << mb / 1024 << "GB";
	return tmp.str();
}

int32_t FileSystemClient::GetIdFromManifest()
{
	std::filesystem::path versionfn = std::filesystem::path(m_repository) / std::filesystem::path(versionFilename);
	std::ofstream versionOFstream;
	versionOFstream.open(versionfn.c_str());
	versionOFstream << "Version " << MANDEYE_VERSION << std::endl;
	versionOFstream << "Git hash " << GIT_HASH << std::endl;

	std::filesystem::path manifest = std::filesystem::path(m_repository) / std::filesystem::path(manifestFilename);
	std::unique_lock<std::mutex> lck(m_mutex);

	std::ifstream manifestFstream;
	manifestFstream.open(manifest.c_str());
	if(manifestFstream.good() && manifestFstream.is_open())
	{
		uint32_t id{0};
		manifestFstream >> id;
		return (id++);
	}
	std::ofstream manifestOFstream;
	manifestOFstream.open(manifest.c_str());
	if(manifestOFstream.good() && manifestOFstream.is_open())
	{
		uint32_t id{0};
		manifestOFstream << id << std::endl;
		return (id++);
	}
	//

	return -1;
}

int32_t FileSystemClient::GetNextIdFromManifest()
{
	std::filesystem::path manifest = std::filesystem::path(m_repository) / std::filesystem::path(manifestFilename);
	int32_t id = GetIdFromManifest();
	id++;
	m_nextId = id;
	std::ofstream manifestOFstream;
	manifestOFstream.open(manifest.c_str());
	if(manifestOFstream.good() && manifestOFstream.is_open())
	{
		manifestOFstream << id << std::endl;
		return id;
	}
	return id;
}

bool FileSystemClient::CreateScanDirectory(const char* dirPrefix, int id, std::filesystem::path& createdDir)
{
	if(!GetIsWritable())
	{
		return false;
	}

	char dirName[256];
	snprintf(dirName, sizeof(dirName), "%s_%04d", dirPrefix, id);
	createdDir = std::filesystem::path(m_repository) / std::filesystem::path(dirName);
	std::cout << "Creating directory " << createdDir.string() << std::endl;

	std::error_code ec;
	std::filesystem::create_directories(createdDir, ec);

	std::unique_lock<std::mutex> lck(m_mutex);
	m_error = ec.message();
	return ec.value() == 0;
}

bool FileSystemClient::CreateDirectoryForContinousScanning(std::string& writable_dir, const int& id_manifest)
{
	std::filesystem::path newDirPath;
	if(!CreateScanDirectory("continousScanning", id_manifest, newDirPath))
	{
		return false;
	}

	// m_currentContinousScanDirectory is read under the same lock by GetDirectories(),
	// which the status thread calls
	std::unique_lock<std::mutex> lck(m_mutex);
	writable_dir = newDirPath.string();
	m_currentContinousScanDirectory = writable_dir;

	// create logfile for buzzer timestamps
	if(m_logBuzzer.is_open())
	{
		m_logBuzzer.close();
	}
	m_logBuzzer.clear();
	m_logBuzzerFilename = newDirPath / buzzerTimestamps;

	return true;
}

bool FileSystemClient::CreateDirectoryForStopScans(std::string& writable_dir, int& id_manifest)
{
	// before the lock: GetNextIdFromManifest() takes m_mutex itself
	id_manifest = GetNextIdFromManifest() - 1;

	std::filesystem::path newDirPath;
	if(!CreateScanDirectory("stopScans", id_manifest, newDirPath))
	{
		return false;
	}

	std::unique_lock<std::mutex> lck(m_mutex);
	writable_dir = newDirPath.string();
	m_currentStopScanDirectory = writable_dir;

	return true;
}

std::vector<std::string> FileSystemClient::GetDirectories()
{
	std::unique_lock<std::mutex> lck(m_mutex);
	std::vector<std::string> fn;
	if(m_currentContinousScanDirectory.empty() || !std::filesystem::exists(m_currentContinousScanDirectory) ||
	   !std::filesystem::is_directory(m_currentContinousScanDirectory))
	{
		// Directory is not set or invalid, return empty vector
		return fn;
	}
	for(const auto& entry : std::filesystem::directory_iterator(m_currentContinousScanDirectory))
	{
		if(entry.is_regular_file())
		{
			auto size = std::filesystem::file_size(entry);
			float fsize = static_cast<float>(size) / (1024 * 1204);
			fn.push_back(entry.path().string() + " " + std::to_string(fsize) + " Mb");
		}
	}

	std::sort(fn.begin(), fn.end());
	return fn;
}

bool FileSystemClient::GetIsWritable()
{
	if(access(m_repository.c_str(), W_OK) == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

nlohmann::json FileSystemClient::GetConfig()
{
	std::filesystem::path configPath = std::filesystem::path(m_repository) / std::filesystem::path(config);
	if(!std::filesystem::exists(configPath))
	{
		std::cerr << "Config file does not exist at " << configPath.string() << std::endl;
		return nlohmann::json();
	}

	nlohmann::json configJson;
	std::ifstream configFile(configPath);
	if(configFile.is_open())
	{
		try
		{
			configFile >> configJson;
		}
		catch(nlohmann::json::parse_error& e)
		{
			std::cerr << "Error parsing config file: " << e.what() << std::endl;
			return nlohmann::json();
		}
		configFile.close();
		return configJson;
	}
	else
	{
		std::cerr << "Failed to open config file at " << configPath.string() << std::endl;
		return nlohmann::json();
	}
	return nlohmann::json();
}
double FileSystemClient::BenchmarkWriteSpeed(const std::string& filename, size_t fileSizeMB)
{
	const size_t bufferSize = 1024 * 1024; // 1 MB buffer
	std::vector<char> buffer(bufferSize, 0xAA);
	std::filesystem::path fileName = std::filesystem::path(m_repository) / std::filesystem::path(filename);
	std::ofstream out(fileName.string(), std::ios::binary);
	if(!out)
	{
		std::cerr << "Failed to open file for writing\n";
		return 0.0;
	}

	auto start = std::chrono::high_resolution_clock::now();
	for(size_t i = 0; i < fileSizeMB; ++i)
	{
		out.write(buffer.data(), bufferSize);
	}
	out.close();
	system("sync");
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed = end - start;
	double mbps = fileSizeMB / elapsed.count();

	std::cout << "Wrote " << fileSizeMB << " MB in " << elapsed.count() << " seconds (" << mbps << " MB/s)\n";
	// clear file
	// Remove the file after benchmarking
	//	std::error_code ec;
	//	std::filesystem::remove(fileName, ec);
	//	if (ec) {
	//		std::cerr << "Failed to remove benchmark file: " << ec.message() << std::endl;
	//	}
	return mbps;
}

bool FileSystemClient::LogBuzzer(uint64_t timestampNs, uint32_t durationMs)
{
	std::unique_lock<std::mutex> lck(m_mutex);
	if(m_logBuzzerFilename.empty())
	{
		// no continous scanning directory created yet, nowhere to log
		return false;
	}

	if(!m_logBuzzer.is_open())
	{
		const bool needsHeader = !std::filesystem::exists(m_logBuzzerFilename);
		m_logBuzzer.open(m_logBuzzerFilename.c_str(), std::ios::out | std::ios::app);
		if(!m_logBuzzer.is_open())
		{
			std::cerr << "Failed to open buzzer log file at " << m_logBuzzerFilename.string() << std::endl;
			return false;
		}
		if(needsHeader)
		{
			m_logBuzzer << "timestampNs,durationMs" << std::endl;
		}
	}

	m_logBuzzer << timestampNs << "," << durationMs << std::endl;
	return m_logBuzzer.good();
}

} // namespace mandeye
