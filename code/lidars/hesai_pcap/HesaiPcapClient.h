#pragma once

#include "lidars/BaseLidarClient.h"
#include "utils/PcapUdpRecorder.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace mandeye
{
//! Hesai PandarXT (XT32 / XT32M2X, UDP protocol 6.1) client that records the raw UDP stream to lidarNNNN.pcap instead of
//! decoding points on the device. Points are produced offline from the pcap files (with the lidar's correction file).
//!
//! Each packet is only inspected, not decoded: its tail gives the lidar time (for isSynced() and the timestamp provider)
//! and the UDP sequence number (for counting packets lost before they reached the recorder).
//!
//! The lidar's own angle correction (PTC command 0x05, CSV "Laser id,Elevation,Azimuth") is saved as correction.dat
//! into every scan directory, so the pcap files can be decoded without the lidar. Lidar IP: MANDEYE_HESAI_LIDAR_IP
//! (default 192.168.1.201, as HesaiClient).
class HesaiPcapClient : public BaseLidarClient
{
public:
	HesaiPcapClient();
	~HesaiPcapClient() override;

	nlohmann::json produceStatus() override;

	//! Opens the lidar UDP port; interfaceIp is not used, the socket listens on all interfaces
	bool startListener(const std::string& interfaceIp) override;
	void stopListener() override;

	//! Nothing to buffer, the pcap files follow startRawChunk() / stopRawChunks()
	void startLog() override { }
	void stopLog() override { }

	//! Always empty buffers: the data is in the pcap files
	std::pair<LidarPointsBufferPtr, LidarIMUBufferPtr> retrieveData() override;

	bool savesRawData() const override
	{
		return true;
	}
	bool savesImuData() const override
	{
		return false;
	}

	void startRawChunk(const std::string& directory, int chunk) override;
	void stopRawChunks() override;

	//! Lidar time within 1 s of the computer time, from packets received in the last second
	bool isSynced() override;

	// TimeStampProvider
	double getTimestamp() override
	{
		return m_lidarTime.load(); // lidar time of the last packet [s]
	}
	double getSessionDuration() override
	{
		return 0.0;
	}
	double getSessionStart() override
	{
		return 0.0;
	}
	void initializeDuration() override { }

private:
	//! Receiver thread of the recorder, for every datagram
	void onPacket(const uint8_t* data, size_t size, const timespec& receiveTime);
	//! Lidar time of a packet [s] from the tail's UTC date and microsecond timestamp
	double packetTime(const uint8_t* tail);
	//! Fetches the correction from the lidar on a worker thread and writes it to directory/correction.dat (empty
	//! directory: only refresh the cached copy). If the lidar does not answer, the last good copy is written.
	void saveCorrectionAsync(const std::string& directory);

	PcapUdpRecorder m_recorder;
	std::string m_lidarIp;

	// correction.dat: m_correctionDirectory is used by the state machine thread only, the rest under m_correctionMutex
	std::string m_correctionDirectory;
	std::thread m_correctionThread;
	mutable std::mutex m_correctionMutex;
	std::vector<uint8_t> m_correctionCache;
	std::string m_correctionFile;
	std::string m_correctionSource;
	std::string m_correctionError;
	size_t m_correctionBytes{0};

	// receiver thread only
	std::array<uint8_t, 6> m_lastUtc{};
	double m_lastUtcSeconds{0.0};
	bool m_haveSequence{false};
	uint32_t m_lastSequence{0};

	// read by status / state machine
	std::atomic<double> m_lidarTime{0.0};
	std::atomic<double> m_timeDiff{0.0};
	std::atomic<int64_t> m_lastPacketSteadyNs{0};
	std::atomic<uint64_t> m_packetsValid{0};
	std::atomic<uint64_t> m_packetsInvalid{0};
	std::atomic<uint64_t> m_sequenceGaps{0};
	std::atomic<uint64_t> m_packetsLostBySequence{0};
	std::atomic<uint64_t> m_sequenceRestarts{0};
	std::atomic<int> m_laserNum{0};
	std::atomic<int> m_blockNum{0};
	std::atomic<int> m_echoNum{0};
	std::atomic<int> m_returnMode{0};
	std::atomic<int> m_motorSpeedRpm{0};
	std::atomic<int> m_packetSize{0};
};
} // namespace mandeye
