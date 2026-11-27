#include "PcapUdpRecorder.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace mandeye
{
namespace
{
#pragma pack(push, 1)
struct PcapGlobalHeader
{
	uint32_t magic;
	uint16_t versionMajor;
	uint16_t versionMinor;
	int32_t thisZone;
	uint32_t sigFigs;
	uint32_t snapLen;
	uint32_t linkType;
};

struct PcapRecordHeader
{
	uint32_t tsSec;
	uint32_t tsNsec;
	uint32_t inclLen;
	uint32_t origLen;
};

//! Synthetic link + network + transport header in front of every datagram (all fields in network byte order).
struct EthIpv4UdpHeader
{
	uint8_t dstMac[6];
	uint8_t srcMac[6];
	uint16_t etherType;
	uint8_t versionIhl;
	uint8_t tos;
	uint16_t totalLength;
	uint16_t id;
	uint16_t flagsFragment;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t checksum;
	uint32_t srcAddr;
	uint32_t dstAddr;
	uint16_t srcPort;
	uint16_t dstPort;
	uint16_t udpLength;
	uint16_t udpChecksum;
};
#pragma pack(pop)
static_assert(sizeof(PcapGlobalHeader) == 24);
static_assert(sizeof(PcapRecordHeader) == 16);
static_assert(sizeof(EthIpv4UdpHeader) == 42);

constexpr uint32_t kPcapMagicNanoseconds = 0xa1b23c4d;
constexpr uint32_t kLinkTypeEthernet = 1;
constexpr int kBatch = 64; //! datagrams per recvmmsg call
constexpr size_t kMaxDatagram = 16384; //! Hesai packets are ~1 kB; longer datagrams are truncated and counted
constexpr size_t kBlockBytes = size_t(1) << 20; //! receiver hands data to the writer in blocks of this size...
constexpr auto kMaxBlockAge = std::chrono::milliseconds(200); //! ...or at least this often
constexpr size_t kMaxRecordBytes = sizeof(PcapRecordHeader) + sizeof(EthIpv4UdpHeader) + kMaxDatagram;
constexpr int kSocketBufferBytes = 64 << 20; //! requested; the kernel caps it at net.core.rmem_max without CAP_NET_ADMIN
constexpr int kSocketBufferWarnBytes = 8 << 20;
constexpr auto kRequestMaxDelay = std::chrono::milliseconds(500);

timespec nowRealtime()
{
	timespec ts{};
	clock_gettime(CLOCK_REALTIME, &ts);
	return ts;
}

bool isBefore(const timespec& a, const timespec& b)
{
	return a.tv_sec < b.tv_sec || (a.tv_sec == b.tv_sec && a.tv_nsec < b.tv_nsec);
}

uint16_t ipv4HeaderChecksum(const EthIpv4UdpHeader& header)
{
	const auto* bytes = reinterpret_cast<const uint8_t*>(&header.versionIhl);
	uint32_t sum = 0;
	for(int i = 0; i < 20; i += 2)
	{
		sum += (uint32_t(bytes[i]) << 8) | bytes[i + 1];
	}
	while(sum >> 16)
	{
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	return htons(static_cast<uint16_t>(~sum));
}

void appendRecord(std::vector<uint8_t>& block,
				  const timespec& ts,
				  const sockaddr_in& source,
				  uint32_t destinationAddress,
				  uint16_t destinationPort,
				  const uint8_t* payload,
				  size_t captured,
				  size_t original)
{
	PcapRecordHeader record{};
	record.tsSec = static_cast<uint32_t>(ts.tv_sec);
	record.tsNsec = static_cast<uint32_t>(ts.tv_nsec);
	record.inclLen = static_cast<uint32_t>(sizeof(EthIpv4UdpHeader) + captured);
	record.origLen = static_cast<uint32_t>(sizeof(EthIpv4UdpHeader) + original);

	EthIpv4UdpHeader header{};
	if(destinationAddress == htonl(INADDR_BROADCAST))
	{
		std::memset(header.dstMac, 0xFF, sizeof(header.dstMac));
	}
	header.etherType = htons(0x0800);
	header.versionIhl = 0x45;
	header.totalLength = htons(static_cast<uint16_t>(20 + 8 + original));
	header.flagsFragment = htons(0x4000); // don't fragment
	header.ttl = 64;
	header.protocol = 17;
	header.srcAddr = source.sin_addr.s_addr;
	header.dstAddr = destinationAddress;
	header.checksum = ipv4HeaderChecksum(header);
	header.srcPort = source.sin_port;
	header.dstPort = htons(destinationPort);
	header.udpLength = htons(static_cast<uint16_t>(8 + original));
	header.udpChecksum = 0; // "no checksum", valid for IPv4

	const size_t offset = block.size();
	block.resize(offset + sizeof(record) + sizeof(header) + captured);
	uint8_t* out = block.data() + offset;
	std::memcpy(out, &record, sizeof(record));
	std::memcpy(out + sizeof(record), &header, sizeof(header));
	std::memcpy(out + sizeof(record) + sizeof(header), payload, captured);
}

bool writeAll(int fd, const void* data, size_t size)
{
	const auto* bytes = static_cast<const uint8_t*>(data);
	while(size > 0)
	{
		const ssize_t written = ::write(fd, bytes, size);
		if(written < 0)
		{
			if(errno == EINTR)
			{
				continue;
			}
			return false;
		}
		bytes += written;
		size -= static_cast<size_t>(written);
	}
	return true;
}
} // namespace

PcapUdpRecorder::PcapUdpRecorder(uint16_t port, size_t maxQueuedBytes)
	: m_port(port)
	, m_maxQueuedBytes(maxQueuedBytes)
{ }

void PcapUdpRecorder::setPacketCallback(PacketCallback callback)
{
	m_packetCallback = std::move(callback);
}

PcapUdpRecorder::~PcapUdpRecorder()
{
	stop();
}

bool PcapUdpRecorder::start()
{
	if(m_running)
	{
		return true;
	}
	m_socket = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if(m_socket < 0)
	{
		reportError("socket");
		return false;
	}
	const int one = 1;
	setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	// SO_RCVBUFFORCE needs CAP_NET_ADMIN, plain SO_RCVBUF is capped by net.core.rmem_max
	const int requestedBuffer = kSocketBufferBytes;
	if(setsockopt(m_socket, SOL_SOCKET, SO_RCVBUFFORCE, &requestedBuffer, sizeof(requestedBuffer)) != 0)
	{
		setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, &requestedBuffer, sizeof(requestedBuffer));
	}
	int receiveBuffer = 0;
	socklen_t optionLength = sizeof(receiveBuffer);
	getsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, &receiveBuffer, &optionLength);
	setsockopt(m_socket, SOL_SOCKET, SO_TIMESTAMPNS, &one, sizeof(one)); // kernel receive time
	setsockopt(m_socket, SOL_SOCKET, SO_RXQ_OVFL, &one, sizeof(one)); // kernel drop counter for this socket
	setsockopt(m_socket, IPPROTO_IP, IP_PKTINFO, &one, sizeof(one)); // destination address of each datagram
	const timeval timeout{0, 100000}; // lets the receiver notice stop() and due requests when no data arrives
	setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_port = htons(m_port);
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(m_socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
	{
		reportError("bind UDP port " + std::to_string(m_port));
		::close(m_socket);
		m_socket = -1;
		return false;
	}
	{
		std::lock_guard<std::mutex> lock(m_statusMutex);
		m_socketReceiveBuffer = receiveBuffer;
	}
	std::cout << "PcapUdpRecorder: listening on UDP port " << m_port << ", socket receive buffer " << receiveBuffer << " B" << std::endl;
	if(receiveBuffer < kSocketBufferWarnBytes)
	{
		std::cerr << "PcapUdpRecorder: small socket receive buffer, raise it with: sysctl -w net.core.rmem_max=" << kSocketBufferBytes << std::endl;
	}
	{
		std::lock_guard<std::mutex> lock(m_requestMutex);
		m_requests.clear();
	}
	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		m_queue.clear();
		m_queuedBytes = 0;
		m_writerStop = false;
	}
	m_running = true;
	m_writerThread = std::thread(&PcapUdpRecorder::writerLoop, this);
	m_receiverThread = std::thread(&PcapUdpRecorder::receiverLoop, this);
	return true;
}

void PcapUdpRecorder::stop()
{
	if(!m_running.exchange(false))
	{
		return;
	}
	if(m_receiverThread.joinable())
	{
		m_receiverThread.join(); // hands its last block and a Close to the writer
	}
	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		m_writerStop = true;
	}
	m_queueCv.notify_all();
	if(m_writerThread.joinable())
	{
		m_writerThread.join();
	}
	::close(m_socket);
	m_socket = -1;
}

void PcapUdpRecorder::startFile(const std::string& path)
{
	Request request;
	request.at = nowRealtime();
	request.requested = std::chrono::steady_clock::now();
	request.path = path;
	pushRequest(std::move(request));
}

void PcapUdpRecorder::closeFile()
{
	Request request;
	request.at = nowRealtime();
	request.requested = std::chrono::steady_clock::now();
	request.close = true;
	pushRequest(std::move(request));
}

void PcapUdpRecorder::pushRequest(Request request)
{
	if(!m_running)
	{
		return; // not started (e.g. the port could not be opened): nothing would consume the request
	}
	std::lock_guard<std::mutex> lock(m_requestMutex);
	m_requests.push_back(std::move(request));
}

void PcapUdpRecorder::enqueue(WriterItem&& item)
{
	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		if(item.kind == WriterItem::Kind::Data)
		{
			if(m_queuedBytes + item.data.size() > m_maxQueuedBytes)
			{
				m_packetsDroppedQueue += item.packets; // the writer can't keep up; files stay valid, only this block is missing
				return;
			}
			m_queuedBytes += item.data.size();
			if(m_queuedBytes > m_queueHighWaterBytes)
			{
				m_queueHighWaterBytes = m_queuedBytes;
			}
		}
		m_queue.push_back(std::move(item));
	}
	m_queueCv.notify_one();
}

void PcapUdpRecorder::reportError(const std::string& what)
{
	const std::string message = what + ": " + std::strerror(errno);
	std::cerr << "PcapUdpRecorder: " << message << std::endl;
	std::lock_guard<std::mutex> lock(m_statusMutex);
	m_lastError = message;
}

void PcapUdpRecorder::receiverLoop()
{
	std::vector<uint8_t> buffers(kBatch * kMaxDatagram);
	std::vector<mmsghdr> messages(kBatch);
	std::vector<iovec> iovecs(kBatch);
	std::vector<sockaddr_in> sources(kBatch);
	struct alignas(cmsghdr) ControlBuffer
	{
		uint8_t data[256];
	};
	std::vector<ControlBuffer> controls(kBatch);

	std::deque<Request> requests; // taken from m_requests, in call order
	bool fileActive = false;
	WriterItem block;
	block.data.reserve(kBlockBytes + kMaxRecordBytes);
	auto blockStart = std::chrono::steady_clock::now();

	auto flushBlock = [&]() {
		if(!block.data.empty())
		{
			enqueue(std::move(block));
			block = WriterItem{};
			block.data.reserve(kBlockBytes + kMaxRecordBytes);
		}
		blockStart = std::chrono::steady_clock::now();
	};
	auto applyRequest = [&](const Request& request) {
		flushBlock();
		WriterItem item;
		if(request.close)
		{
			if(!fileActive)
			{
				return;
			}
			item.kind = WriterItem::Kind::Close;
			fileActive = false;
		}
		else
		{
			item.kind = WriterItem::Kind::Open; // the writer closes the previous file first
			item.path = request.path;
			fileActive = true;
		}
		enqueue(std::move(item));
	};
	// Applies the requests made before a datagram received at `ts`. A request older than kRequestMaxDelay is applied
	// anyway, so a step of the realtime clock cannot hold a file open.
	auto applyDueRequests = [&](const timespec& ts) {
		const auto now = std::chrono::steady_clock::now();
		while(!requests.empty() && (!isBefore(ts, requests.front().at) || now - requests.front().requested > kRequestMaxDelay))
		{
			applyRequest(requests.front());
			requests.pop_front();
		}
	};

	while(m_running)
	{
		{
			std::lock_guard<std::mutex> lock(m_requestMutex);
			while(!m_requests.empty())
			{
				requests.push_back(std::move(m_requests.front()));
				m_requests.pop_front();
			}
		}
		for(int i = 0; i < kBatch; ++i)
		{
			iovecs[i].iov_base = buffers.data() + i * kMaxDatagram;
			iovecs[i].iov_len = kMaxDatagram;
			msghdr& header = messages[i].msg_hdr;
			header = msghdr{};
			header.msg_name = &sources[i];
			header.msg_namelen = sizeof(sockaddr_in);
			header.msg_iov = &iovecs[i];
			header.msg_iovlen = 1;
			header.msg_control = controls[i].data;
			header.msg_controllen = sizeof(controls[i].data);
			messages[i].msg_len = 0;
		}
		// MSG_WAITFORONE: block (up to SO_RCVTIMEO) for the first datagram only; MSG_TRUNC: msg_len is the real length
		const int received = recvmmsg(m_socket, messages.data(), kBatch, MSG_WAITFORONE | MSG_TRUNC, nullptr);
		if(received < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
		{
			reportError("recvmmsg");
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		for(int i = 0; i < received; ++i)
		{
			msghdr& header = messages[i].msg_hdr;
			timespec ts{};
			bool hasTimestamp = false;
			uint32_t destinationAddress = 0;
			for(cmsghdr* control = CMSG_FIRSTHDR(&header); control != nullptr; control = CMSG_NXTHDR(&header, control))
			{
				if(control->cmsg_level == SOL_SOCKET && control->cmsg_type == SCM_TIMESTAMPNS)
				{
					std::memcpy(&ts, CMSG_DATA(control), sizeof(ts));
					hasTimestamp = true;
				}
				else if(control->cmsg_level == SOL_SOCKET && control->cmsg_type == SO_RXQ_OVFL)
				{
					uint32_t drops = 0; // cumulative for this socket
					std::memcpy(&drops, CMSG_DATA(control), sizeof(drops));
					m_packetsDroppedKernel = drops;
				}
				else if(control->cmsg_level == IPPROTO_IP && control->cmsg_type == IP_PKTINFO)
				{
					in_pktinfo info{};
					std::memcpy(&info, CMSG_DATA(control), sizeof(info));
					destinationAddress = info.ipi_addr.s_addr;
				}
			}
			if(!hasTimestamp)
			{
				ts = nowRealtime();
			}
			applyDueRequests(ts);
			m_packetsReceived++;
			if(m_packetCallback)
			{
				m_packetCallback(buffers.data() + i * kMaxDatagram, std::min<size_t>(messages[i].msg_len, kMaxDatagram), ts);
			}
			if(!fileActive)
			{
				m_packetsDiscardedNoFile++;
				continue;
			}
			const size_t original = messages[i].msg_len;
			const size_t captured = std::min(original, kMaxDatagram);
			if(captured < original)
			{
				m_packetsTruncated++;
			}
			appendRecord(block.data, ts, sources[i], destinationAddress, m_port, buffers.data() + i * kMaxDatagram, captured, original);
			block.packets++;
			if(block.data.size() >= kBlockBytes)
			{
				flushBlock();
			}
		}
		// Socket drained (or timed out): what is still due by wall clock can't misplace a datagram anymore.
		if(received < kBatch)
		{
			applyDueRequests(nowRealtime());
		}
		if(std::chrono::steady_clock::now() - blockStart > kMaxBlockAge)
		{
			flushBlock();
		}
	}
	flushBlock();
	if(fileActive)
	{
		WriterItem close;
		close.kind = WriterItem::Kind::Close;
		enqueue(std::move(close));
	}
}

void PcapUdpRecorder::writerLoop()
{
	int fd = -1;
	auto finishFile = [&]() {
		if(fd < 0)
		{
			return;
		}
		if(fdatasync(fd) != 0)
		{
			m_writeErrors++;
			reportError("fdatasync");
		}
		::close(fd);
		fd = -1;
		m_filesWritten++;
		std::lock_guard<std::mutex> lock(m_statusMutex);
		m_currentFile.clear();
	};
	auto abandonFile = [&](const std::string& what) {
		m_writeErrors++;
		reportError(what);
		::close(fd);
		fd = -1;
		std::lock_guard<std::mutex> lock(m_statusMutex);
		m_currentFile.clear();
	};

	while(true)
	{
		WriterItem item;
		{
			std::unique_lock<std::mutex> lock(m_queueMutex);
			m_queueCv.wait(lock, [&] { return !m_queue.empty() || m_writerStop; });
			if(m_queue.empty())
			{
				break; // stop requested and everything written
			}
			item = std::move(m_queue.front());
			m_queue.pop_front();
			if(item.kind == WriterItem::Kind::Data)
			{
				m_queuedBytes -= item.data.size();
			}
		}

		if(item.kind == WriterItem::Kind::Open)
		{
			finishFile();
			fd = ::open(item.path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
			if(fd < 0)
			{
				m_writeErrors++;
				reportError("open " + item.path);
				continue;
			}
			const PcapGlobalHeader header{kPcapMagicNanoseconds, 2, 4, 0, 0, 65535, kLinkTypeEthernet};
			if(!writeAll(fd, &header, sizeof(header)))
			{
				abandonFile("write " + item.path);
				continue;
			}
			std::lock_guard<std::mutex> lock(m_statusMutex);
			m_currentFile = item.path;
		}
		else if(item.kind == WriterItem::Kind::Data)
		{
			if(fd < 0)
			{
				m_packetsLostWriteError += item.packets; // the file could not be opened or a write failed
				continue;
			}
			if(!writeAll(fd, item.data.data(), item.data.size()))
			{
				m_packetsLostWriteError += item.packets;
				abandonFile("write");
				continue;
			}
			m_packetsWritten += item.packets;
			m_bytesWritten += item.data.size();
		}
		else
		{
			finishFile();
		}
	}
	finishFile();
}

nlohmann::json PcapUdpRecorder::produceStatus() const
{
	nlohmann::json status;
	status["port"] = m_port;
	status["running"] = m_running.load();
	{
		std::lock_guard<std::mutex> lock(m_statusMutex);
		status["file"] = m_currentFile;
		status["last_error"] = m_lastError;
		status["socket_rcvbuf_bytes"] = m_socketReceiveBuffer;
	}
	status["packets_received"] = m_packetsReceived.load();
	status["packets_written"] = m_packetsWritten.load();
	status["packets_discarded_no_file"] = m_packetsDiscardedNoFile.load();
	status["packets_dropped_queue"] = m_packetsDroppedQueue.load();
	status["packets_dropped_kernel"] = m_packetsDroppedKernel.load();
	status["packets_lost_write_error"] = m_packetsLostWriteError.load();
	status["packets_truncated"] = m_packetsTruncated.load();
	status["bytes_written"] = m_bytesWritten.load();
	status["files_written"] = m_filesWritten.load();
	status["write_errors"] = m_writeErrors.load();
	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		status["queue_bytes"] = m_queuedBytes;
	}
	status["queue_high_water_bytes"] = m_queueHighWaterBytes.load();
	return status;
}
} // namespace mandeye
