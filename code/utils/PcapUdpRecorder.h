#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <ctime>
#include <deque>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

namespace mandeye
{
//! Records the raw UDP datagrams arriving on a port into .pcap files.
//!
//! - A plain UDP socket is used, so no root / CAP_NET_RAW is needed.
//! - The socket sets SO_REUSEADDR, so it can listen on the same port as a lidar SDK. Both sockets get every datagram only
//!   when the lidar sends to a broadcast or multicast address (Hesai default: 255.255.255.255). With a unicast
//!   destination Linux delivers each datagram to just one of the sockets.
//! - Each datagram is stored behind a synthetic 42-byte Ethernet/IPv4/UDP header carrying the real source and destination
//!   address and ports, so files open in Wireshark/tcpdump and in the Hesai SDK PcapSource. Timestamps are the kernel
//!   receive time (CLOCK_REALTIME, nanosecond pcap format).
//! - A receiver thread only copies datagrams into memory blocks, a writer thread does the disk I/O, so a slow drive does
//!   not stall the socket. Memory in flight is bounded by maxQueuedBytes; datagrams over that limit are counted as dropped.
class PcapUdpRecorder
{
public:
	explicit PcapUdpRecorder(uint16_t port, size_t maxQueuedBytes = size_t(128) << 20);
	~PcapUdpRecorder();
	PcapUdpRecorder(const PcapUdpRecorder&) = delete;
	PcapUdpRecorder& operator=(const PcapUdpRecorder&) = delete;

	//! Called on the receiver thread for every datagram, also when no file is open; must be quick.
	using PacketCallback = std::function<void(const uint8_t* data, size_t size, const timespec& receiveTime)>;
	//! Set before start().
	void setPacketCallback(PacketCallback callback);

	//! Called on the writer thread when a file is finished: ok is true when the file was opened, every write and the final
	//! fdatasync succeeded, and it holds at least one datagram. Also called with ok == false when a file can't be opened or a
	//! write fails. Must be quick (it holds up the writer). Set before start().
	using FileFinishedCallback = std::function<void(const std::string& path, bool ok, uint64_t packets)>;
	void setFileFinishedCallback(FileFinishedCallback callback);

	//! Opens the socket and starts the threads. Datagrams are discarded until startFile() is called.
	bool start();
	//! Stops the threads, writes out everything received so far and closes the current file.
	void stop();
	//! Datagrams received from now on go to a new file at path. The previous file, if any, is finished and closed.
	//! The switch happens on the kernel receive timestamp, so no datagram is lost or duplicated between files.
	void startFile(const std::string& path);
	//! Finishes and closes the current file. Datagrams are discarded until the next startFile().
	void closeFile();

	nlohmann::json produceStatus() const;

private:
	struct Request
	{
		timespec at{}; //! CLOCK_REALTIME when requested; datagrams received at or after it follow the request
		std::chrono::steady_clock::time_point requested; //! fallback if the realtime clock steps
		bool close{false};
		std::string path;
	};

	struct WriterItem
	{
		enum class Kind
		{
			Open,
			Data,
			Close
		};
		Kind kind{Kind::Data};
		std::string path;
		std::vector<uint8_t> data;
		uint64_t packets{0};
	};

	void receiverLoop();
	void writerLoop();
	void pushRequest(Request request);
	void enqueue(WriterItem&& item);
	void reportError(const std::string& what);

	const uint16_t m_port;
	const size_t m_maxQueuedBytes;
	int m_socket{-1};
	PacketCallback m_packetCallback;
	FileFinishedCallback m_fileFinishedCallback;
	std::atomic<bool> m_running{false};
	std::thread m_receiverThread;
	std::thread m_writerThread;

	// startFile()/closeFile() -> receiver thread
	std::mutex m_requestMutex;
	std::deque<Request> m_requests;

	// receiver thread -> writer thread
	mutable std::mutex m_queueMutex;
	std::condition_variable m_queueCv;
	std::deque<WriterItem> m_queue;
	size_t m_queuedBytes{0};
	bool m_writerStop{false};

	// status
	mutable std::mutex m_statusMutex;
	std::string m_currentFile;
	std::string m_lastError;
	int m_socketReceiveBuffer{0};
	std::atomic<uint64_t> m_packetsReceived{0};
	std::atomic<uint64_t> m_packetsWritten{0};
	std::atomic<uint64_t> m_packetsDiscardedNoFile{0};
	std::atomic<uint64_t> m_packetsDroppedQueue{0};
	std::atomic<uint64_t> m_packetsDroppedKernel{0};
	std::atomic<uint64_t> m_packetsLostWriteError{0};
	std::atomic<uint64_t> m_packetsTruncated{0};
	std::atomic<uint64_t> m_bytesWritten{0};
	std::atomic<uint64_t> m_filesWritten{0};
	std::atomic<uint64_t> m_writeErrors{0};
	std::atomic<uint64_t> m_queueHighWaterBytes{0};
};
} // namespace mandeye
