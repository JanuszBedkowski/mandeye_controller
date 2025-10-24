#include "FileSystemClient.h"
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
	std::filesystem::path versionfn =
		std::filesystem::path(m_repository) / std::filesystem::path(versionFilename);
	std::ofstream versionOFstream;
	versionOFstream.open(versionfn.c_str());
	versionOFstream << "Version 0.6-dev" << std::endl;
	
	std::filesystem::path manifest =
		std::filesystem::path(m_repository) / std::filesystem::path(manifestFilename);
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
	std::filesystem::path manifest =
		std::filesystem::path(m_repository) / std::filesystem::path(manifestFilename);
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

bool FileSystemClient::CreateDirectoryForContinousScanning(std::string &writable_dir, const int &id_manifest)
{
	std::string ret;

	if(GetIsWritable())
	{
		//auto id = GetNextIdFromManifest();
		auto id = id_manifest;
		char dirName[256];
		snprintf(dirName, 256, "continousScanning_%04d", id);
		std::filesystem::path newDirPath =
			std::filesystem::path(m_repository) / std::filesystem::path(dirName);
		std::cout << "Creating directory " << newDirPath.string() << std::endl;
		std::error_code ec;
		std::filesystem::create_directories(newDirPath, ec);
		m_error = ec.message();
		if(ec.value() == 0)
		{
			if(!newDirPath.string().empty()){
				writable_dir = newDirPath.string();
			        m_currentContinousScanDirectory = writable_dir;
				return true;
			}else{
				return false;
			}
		}else{
			return false;
		}
	}else{
		return false;
	}
}

bool FileSystemClient::CreateDirectoryForStopScans(std::string &writable_dir, int &id_manifest){
	std::string ret;

	if(GetIsWritable())
	{
		id_manifest = GetNextIdFromManifest() - 1;
		char dirName[256];
		snprintf(dirName, 256, "stopScans_%04d", id_manifest);
		std::filesystem::path newDirPath =
			std::filesystem::path(m_repository) / std::filesystem::path(dirName);
		std::cout << "Creating directory " << newDirPath.string() << std::endl;
		std::error_code ec;
		std::filesystem::create_directories(newDirPath, ec);
		m_error = ec.message();
		if(ec.value() == 0)
		{
			if(!newDirPath.string().empty()){
				writable_dir = newDirPath.string();
			        m_currentStopScanDirectory = writable_dir;
				return true;
			}else{
				return false;
			}
		}else{
			return false;
		}
	}else{
		return false;
	}
}

std::vector<std::string> FileSystemClient::GetDirectories()
{
	std::unique_lock<std::mutex> lck(m_mutex);
	std::vector<std::string> fn;
	if (m_currentContinousScanDirectory.empty() ||
	    !std::filesystem::exists(m_currentContinousScanDirectory) ||
	    !std::filesystem::is_directory(m_currentContinousScanDirectory)) {
	    // Directory is not set or invalid, return empty vector
	    return fn;
	}
	for(const auto& entry : std::filesystem::directory_iterator(m_currentContinousScanDirectory))
	{
	    if(entry.is_regular_file() )
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
	if (!std::filesystem::exists(configPath))
	{
		std::cerr << "Config file does not exist at " << configPath.string() << std::endl;
		return nlohmann::json();
	}

	nlohmann::json configJson;
	std::ifstream configFile(configPath);
	if (configFile.is_open())
	{
		try
		{
			configFile >> configJson;
		}
		catch (nlohmann::json::parse_error& e)
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
double FileSystemClient::BenchmarkWriteSpeed(const std::string& filename, size_t fileSizeMB) {
	const size_t bufferSize = 1024 * 1024; // 1 MB buffer
	std::vector<char> buffer(bufferSize, 0xAA);
	std::filesystem::path fileName = std::filesystem::path(m_repository)/ std::filesystem::path(filename);
	std::ofstream out(fileName.string(), std::ios::binary);
	if (!out) {
		std::cerr << "Failed to open file for writing\n";
		return 0.0;
	}

	auto start = std::chrono::high_resolution_clock::now();
	for (size_t i = 0; i < fileSizeMB; ++i) {
		out.write(buffer.data(), bufferSize);
	}
	out.close();
	auto end = std::chrono::high_resolution_clock::now();
	system("sync");
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

} // namespace mandeye
