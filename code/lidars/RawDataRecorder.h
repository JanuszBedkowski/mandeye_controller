#pragma once
#include <string>

namespace mandeye
{
//! Optional interface of lidar clients that record the raw sensor stream themselves (e.g. HESAI_PCAP) instead of
//! returning points. main.cpp finds it with dynamic_cast; clients without it return points and get LAZ files.
//!
//! Deliberately not part of BaseLidarClient: new virtual methods there change the vtable of every client, and lidar
//! plugins loaded at runtime (libhesai_lib.so, liblivox2_lib.so, ...) built against the older header then crash when
//! a new method is called (seen with an Oct 2026 libhesai_lib.so: SIGSEGV at scan start). Keep BaseLidarClient's
//! virtual methods unchanged, or rebuild and install every plugin together with control_program.
class RawDataRecorder
{
public:
	virtual ~RawDataRecorder() = default;

	//! Start writing the files of a chunk. Called when a scan starts and at every chunk cut.
	virtual void startRawChunk(const std::string& directory, int chunk) = 0;

	//! Finish the current chunk. Called when a scan stops.
	virtual void stopRawChunks() = 0;
};
} // namespace mandeye
