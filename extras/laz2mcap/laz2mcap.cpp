#include "lidars/replay/ReplayLidar.h"
#include <filesystem>
#include <iostream>

int main(int argc, char* argv[])
{
	if(argc < 2)
	{
		std::cerr << "Usage: laz2mcap <directory>\n"
				  << "  Converts lidar<N>.laz + imu<N>.csv files in <directory>\n"
				  << "  to a single session.mcap (skips if already present).\n";
		return 1;
	}

	namespace fs = std::filesystem;
	const fs::path dir(argv[1]);

	if(!fs::is_directory(dir))
	{
		std::cerr << "laz2mcap: not a directory: " << dir << "\n";
		return 1;
	}

	const fs::path mcap = dir / "session.mcap";
	if(fs::exists(mcap))
	{
		std::cout << "laz2mcap: session.mcap already exists, nothing to do.\n";
		return 0;
	}

	const fs::path result = mandeye::ReplayLidar::convertToMcap(dir);
	if(result.empty())
	{
		std::cerr << "laz2mcap: conversion failed (no laz files found in " << dir << ")\n";
		return 1;
	}

	std::cout << "laz2mcap: done → " << result << "\n";
	return 0;
}