#include "lidars/hesai_pcap/HesaiPcapClient.h"
#include "save_data.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

namespace mandeye
{
namespace
{
constexpr uint16_t kHesaiUdpPort = 2368; // same as HesaiClient

// PandarXT UDP protocol 6.1 (hesai_sdk libhesai/UdpProtocol/udp_protocol_v6_1.h)
constexpr size_t kPreHeaderSize = 6; // 0xEE 0xFF, version major, minor, status info version, reserved
constexpr size_t kHeaderSize = 6; // laser num, block num, first block return, dist unit, echo num, flags
constexpr size_t kAzimuthSize = 2;
constexpr size_t kChannelSize = 4; // distance (2), reflectivity, confidence
constexpr size_t kTailSize = 28;
constexpr uint8_t kFlagSequenceNumber = 0x01;
// offsets in the tail: 3 x reserved (3 B each), shutdown, return mode, motor speed, UTC (6), timestamp, factory info, sequence
constexpr size_t kTailReturnMode = 10;
constexpr size_t kTailMotorSpeed = 11;
constexpr size_t kTailUtc = 13;
constexpr size_t kTailTimestamp = 19;
constexpr size_t kTailSequence = 24;

constexpr double kSyncedMaxDiffSec = 1.0; // same criterion as HesaiClient
constexpr auto kSyncedMaxPacketAge = std::chrono::seconds(1);
constexpr uint32_t kSequenceRestartThreshold = 1000000; // a larger jump is a lidar restart, not a loss

uint16_t readLe16(const uint8_t* p)
{
	return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t readLe32(const uint8_t* p)
{
	return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

int64_t steadyNowNs()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
} // namespace

HesaiPcapClient::HesaiPcapClient()
	: m_recorder(kHesaiUdpPort)
{
	m_recorder.setPacketCallback([this](const uint8_t* data, size_t size, const timespec& receiveTime) { onPacket(data, size, receiveTime); });
}

HesaiPcapClient::~HesaiPcapClient()
{
	m_recorder.stop();
}

bool HesaiPcapClient::startListener(const std::string& interfaceIp)
{
	std::cout << "HesaiPcapClient: recording raw UDP from port " << kHesaiUdpPort << " (interface " << interfaceIp << " not used, listening on all)"
			  << std::endl;
	return m_recorder.start();
}

void HesaiPcapClient::stopListener()
{
	m_recorder.stop();
}

std::pair<LidarPointsBufferPtr, LidarIMUBufferPtr> HesaiPcapClient::retrieveData()
{
	return {std::make_shared<LidarPointsBuffer>(), std::make_shared<LidarIMUBuffer>()};
}

void HesaiPcapClient::startRawChunk(const std::string& directory, int chunk)
{
	m_recorder.startFile(getPcapFilePath(directory, chunk));
}

void HesaiPcapClient::stopRawChunks()
{
	m_recorder.closeFile();
}

bool HesaiPcapClient::isSynced()
{
	const bool fresh = steadyNowNs() - m_lastPacketSteadyNs.load() < std::chrono::nanoseconds(kSyncedMaxPacketAge).count();
	return m_packetsValid.load() > 0 && fresh && m_timeDiff.load() < kSyncedMaxDiffSec;
}

double HesaiPcapClient::packetTime(const uint8_t* tail)
{
	const uint8_t* utc = tail + kTailUtc;
	const double micros = readLe32(tail + kTailTimestamp);
	if(utc[0] != 0)
	{
		// year-1900, month, day, hour, minute, second; mktime only when the second changes
		if(!std::equal(m_lastUtc.begin(), m_lastUtc.end(), utc))
		{
			std::copy(utc, utc + m_lastUtc.size(), m_lastUtc.begin());
			std::tm t{};
			t.tm_year = utc[0] < 70 ? utc[0] + 100 : utc[0];
			t.tm_mon = utc[1] - 1;
			t.tm_mday = utc[2];
			t.tm_hour = utc[3];
			t.tm_min = utc[4];
			t.tm_sec = utc[5];
			m_lastUtcSeconds = static_cast<double>(timegm(&t));
		}
		return m_lastUtcSeconds + micros * 1e-6;
	}
	// otherwise bytes 2..5 hold Unix seconds, big endian
	const uint32_t seconds = (uint32_t(utc[2]) << 24) | (uint32_t(utc[3]) << 16) | (uint32_t(utc[4]) << 8) | uint32_t(utc[5]);
	return seconds + micros * 1e-6;
}

void HesaiPcapClient::onPacket(const uint8_t* data, size_t size, const timespec& receiveTime)
{
	if(size < kPreHeaderSize + kHeaderSize + kTailSize || data[0] != 0xEE || data[1] != 0xFF)
	{
		m_packetsInvalid++; // not a point cloud packet (or another sender on the port)
		return;
	}
	const uint8_t* header = data + kPreHeaderSize;
	const int laserNum = header[0];
	const int blockNum = header[1];
	const size_t tailOffset = kPreHeaderSize + kHeaderSize + size_t(blockNum) * (kAzimuthSize + kChannelSize * size_t(laserNum));
	if(tailOffset + kTailSize > size)
	{
		m_packetsInvalid++;
		return;
	}
	const uint8_t* tail = data + tailOffset;

	const double lidarTime = packetTime(tail);
	const double receive = double(receiveTime.tv_sec) + double(receiveTime.tv_nsec) * 1e-9;
	m_lidarTime = lidarTime;
	m_timeDiff = std::abs(receive - lidarTime);
	m_lastPacketSteadyNs = steadyNowNs();

	if(header[5] & kFlagSequenceNumber)
	{
		const uint32_t sequence = readLe32(tail + kTailSequence);
		if(m_haveSequence)
		{
			const uint32_t missing = sequence - (m_lastSequence + 1); // unsigned: handles wrap-around
			if(missing != 0)
			{
				if(missing < kSequenceRestartThreshold)
				{
					m_sequenceGaps++;
					m_packetsLostBySequence += missing;
				}
				else
				{
					m_sequenceRestarts++;
				}
			}
		}
		m_lastSequence = sequence;
		m_haveSequence = true;
	}

	m_laserNum = laserNum;
	m_blockNum = blockNum;
	m_echoNum = header[4];
	m_returnMode = tail[kTailReturnMode];
	m_motorSpeedRpm = readLe16(tail + kTailMotorSpeed);
	m_packetSize = static_cast<int>(size);
	m_packetsValid++;
}

nlohmann::json HesaiPcapClient::produceStatus()
{
	nlohmann::json status;
	status["time_diff"] = m_timeDiff.load();
	status["timestamp_sec"] = m_lidarTime.load();
	status["packets_valid"] = m_packetsValid.load();
	status["packets_invalid"] = m_packetsInvalid.load();
	status["sequence_gaps"] = m_sequenceGaps.load();
	status["packets_lost_by_sequence"] = m_packetsLostBySequence.load();
	status["sequence_restarts"] = m_sequenceRestarts.load();
	status["laser_num"] = m_laserNum.load();
	status["block_num"] = m_blockNum.load();
	status["echo_num"] = m_echoNum.load();
	status["return_mode"] = m_returnMode.load();
	status["motor_speed_rpm"] = m_motorSpeedRpm.load();
	status["packet_size"] = m_packetSize.load();

	nlohmann::json data;
	data["is_synced"] = isSynced();
	data["HesaiPcapClient"]["status"] = status;
	data["HesaiPcapClient"]["recorder"] = m_recorder.produceStatus();
	data["counters"]["lidar"] = m_packetsValid.load();
	data["counters"]["imu"] = 0;
	return data;
}
} // namespace mandeye
