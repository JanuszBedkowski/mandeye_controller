#include <nlohmann/json.hpp>
#include <zmq.hpp>
#include <libserial/SerialPort.h>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <mutex>
#include <deque>
#include <iostream>
#include <cstdint>
#include <cmath>
#include <cstring>

namespace fs = std::filesystem;

// ── Packet definition (must match firmware imu_pkt_t) ────────────────────────
#pragma pack(push, 1)
struct ImuPacket {
    uint8_t  sync;
    uint64_t timestamp_us;
    int32_t  rate[3];
    int32_t  acc[3];
    int32_t  temp;
    uint8_t  crc;
};
#pragma pack(pop)

static constexpr size_t  PKT_SIZE         = sizeof(ImuPacket);
static constexpr uint8_t SYNC_BYTE        = 0xAA;
static constexpr double  SENSITIVITY_RATE = 1600.0;
static constexpr double  SENSITIVITY_ACC  = 3200.0;

static uint8_t crc8(const uint8_t* data, size_t len)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : crc << 1;
    }
    return crc;
}

inline double deg2rad(double deg) { return deg * M_PI / 180.0; }

// ── Config / globals ─────────────────────────────────────────────────────────
namespace {
    std::string getEnvString(const std::string& env, const std::string& def) {
        const char* p = std::getenv(env.c_str());
        return p ? std::string{p} : def;
    }
}

namespace global {
    std::string directoryName = "MURATA_IMU";
    int         fileLengthMs  = 20000;
    std::string uartPort      = "/dev/ttyUSB0";
}

// ── Mode state ───────────────────────────────────────────────────────────────
namespace MODES {
    const static char* SCANNING = "SCANNING";
    const static char* STOPPING = "STOPPING";
    const static char* UNKNOWN  = "UNKNOWN";

    const auto SCANNING_ID = std::hash<std::string>{}(SCANNING);
    const auto STOPPING_ID = std::hash<std::string>{}(STOPPING);
    const auto UNKNOWN_ID  = std::hash<std::string>{}(UNKNOWN);
}

namespace state {
    std::mutex  stateMutex;
    std::string modeName            = MODES::UNKNOWN;
    auto        hashCode            = MODES::UNKNOWN_ID;
    std::string continuousScanTarget;
    uint64_t    timestamp;
}

// ── Config ───────────────────────────────────────────────────────────────────
nlohmann::json getConfig(const std::string& configPath)
{
    if (!fs::exists(configPath)) {
        std::cerr << "Config file does not exist" << std::endl;
        return {};
    }
    std::ifstream f(configPath);
    if (!f.is_open()) {
        std::cerr << "Failed to open config at " << configPath << std::endl;
        return {};
    }
    try {
        nlohmann::json j; f >> j; return j;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "Config parse error: " << e.what() << std::endl;
        return {};
    }
}

// ── ZMQ subscriber thread ────────────────────────────────────────────────────
void clientThread()
{
    try {
        zmq::context_t ctx(1);
        zmq::socket_t  sock(ctx, zmq::socket_type::sub);
        sock.connect("tcp://localhost:5556");
        sock.set(zmq::sockopt::subscribe, "");
        sock.set(zmq::sockopt::conflate, 1);
        std::cout << "Connected to tcp://localhost:5556" << std::endl;

        while (true) {
            zmq::message_t msg;
            if (!sock.recv(msg, zmq::recv_flags::none)) continue;

            auto j = nlohmann::json::parse(
                std::string(static_cast<char*>(msg.data()), msg.size()));
            if (!j.is_object()) continue;

            std::lock_guard<std::mutex> lk(state::stateMutex);
            if (j.contains("time"))
                state::timestamp = j["time"].get<uint64_t>();
            if (j.contains("mode")) {
                state::modeName = j["mode"].get<std::string>();
                state::hashCode = std::hash<std::string>{}(state::modeName);
            }
            if (j.contains("continousScanDirectory"))
                state::continuousScanTarget = j["continousScanDirectory"].get<std::string>();
        }
    } catch (const zmq::error_t& e) {
        std::cerr << "ZMQ error: " << e.what() << std::endl;
        std::abort();
    }
}

// ── Row stored in memory ──────────────────────────────────────────────────────
struct ImuRow {
    uint64_t ts_ns;
    double gx, gy, gz;
    double ax, ay, az;
};

// ── Flush buffer to file in a detached thread ─────────────────────────────────
void saveToFile(std::deque<ImuRow> rows, std::string path)
{
    std::ofstream f(path);
    if (!f) { std::cerr << "Failed to open " << path << std::endl; return; }
    f << "timestamp gyroX gyroY gyroZ accX accY accZ timestampUnix\n";
    for (const auto& r : rows)
        f << r.ts_ns << ' '
          << r.gx << ' ' << r.gy << ' ' << r.gz << ' '
          << r.ax << ' ' << r.ay << ' ' << r.az << ' '
          << r.ts_ns << '\n';
    std::cout << "Saved " << rows.size() << " rows to " << path << std::endl;
}

// ── IMU UART thread ──────────────────────────────────────────────────────────
int murataThread()
{
    LibSerial::SerialPort serial;
    try {
        serial.Open(global::uartPort);
        serial.SetBaudRate(LibSerial::BaudRate::BAUD_460800);
        serial.SetCharacterSize(LibSerial::CharacterSize::CHAR_SIZE_8);
        serial.SetParity(LibSerial::Parity::PARITY_NONE);
        serial.SetStopBits(LibSerial::StopBits::STOP_BITS_1);
        serial.SetFlowControl(LibSerial::FlowControl::FLOW_CONTROL_NONE);
    } catch (const LibSerial::OpenFailed&) {
        std::cerr << "Failed to open " << global::uartPort << std::endl;
        return -1;
    }
    std::cout << "Opened " << global::uartPort << " @ 460800" << std::endl;

    std::deque<ImuRow> buffer;
    auto fileStartTime = std::chrono::steady_clock::now();
    auto statsTime     = std::chrono::steady_clock::now();
    int  pktCount = 0, errCount = 0;
    bool collecting = false;

    uint8_t buf[PKT_SIZE];

    for (;;) {
        // ── Sync on 0xAA ──────────────────────────────────────────────────────
        try {
            uint8_t b;
            serial.ReadByte(b, 0);
            if (b != SYNC_BYTE) { errCount++; continue; }
            buf[0] = b;
            for (size_t i = 1; i < PKT_SIZE; i++)
                serial.ReadByte(buf[i], 0);
        } catch (const LibSerial::ReadTimeout&) { continue; }

        if (crc8(buf, PKT_SIZE - 1) != buf[PKT_SIZE - 1]) { errCount++; continue; }

        ImuPacket pkt;
        memcpy(&pkt, buf, PKT_SIZE);
        pktCount++;

        // ── Decode into row ───────────────────────────────────────────────────
        ImuRow row;
        row.ts_ns = pkt.timestamp_us * 1000ULL;
        row.gx    = deg2rad(pkt.rate[0] / SENSITIVITY_RATE);
        row.gy    = deg2rad(pkt.rate[1] / SENSITIVITY_RATE);
        row.gz    = deg2rad(pkt.rate[2] / SENSITIVITY_RATE);
        row.ax    = pkt.acc[0] / SENSITIVITY_ACC;
        row.ay    = pkt.acc[1] / SENSITIVITY_ACC;
        row.az    = pkt.acc[2] / SENSITIVITY_ACC;

        // ── Read mode ─────────────────────────────────────────────────────────
        size_t      state_id;
        std::string scanTarget;
        {
            std::lock_guard<std::mutex> lk(state::stateMutex);
            state_id   = state::hashCode;
            scanTarget = state::continuousScanTarget;
        }

        // ── Accumulate to memory ──────────────────────────────────────────────
        if (state_id == MODES::SCANNING_ID) {
            if (!collecting) {
                collecting    = true;
                fileStartTime = std::chrono::steady_clock::now();
            }
            buffer.push_back(row);
        }

        // ── Flush: time rotation or stop ──────────────────────────────────────
        const bool timeUp   = collecting &&
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - fileStartTime).count() > global::fileLengthMs;
        const bool stopping = collecting && (state_id == MODES::STOPPING_ID);

        if (timeUp || stopping) {
            const fs::path dir = fs::path(scanTarget) / global::directoryName;
            fs::create_directories(dir);
            std::string path = (dir / ("murata_imu_" + std::to_string(pkt.timestamp_us) + ".csv")).string();

            // Move buffer ownership to a detached thread — no disk I/O on this thread
            std::thread(saveToFile, std::move(buffer), std::move(path)).detach();
            buffer = {};

            if (stopping)
                collecting = false;
            else
                fileStartTime = std::chrono::steady_clock::now();
        }

        // ── Stats ─────────────────────────────────────────────────────────────
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - statsTime).count() >= 1) {
            double tp     = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
            double imu_ts = double(pkt.timestamp_us) / 1000000.0;
            std::cout << "pkts/s: " << pktCount
                      << "  errors: " << errCount
                      << "  diff: "   << (tp - imu_ts)
                      << "  buffered: " << buffer.size()
                      << std::endl;
            pktCount = errCount = 0;
            statsTime = now;
        }
    }
    return -1;
}

// ── Main ─────────────────────────────────────────────────────────────────────
int main(int argc, char** argv)
{
    std::string configPath = getEnvString("EXTRA_MURATA_CONFIG_PATH", "/media/usb/config_murata_imu.json");
    global::directoryName  = getEnvString("EXTRA_MURATA_DIRECTORY_NAME", "MURATA_IMU");
    global::uartPort       = getEnvString("EXTRA_MURATA_UART_PORT", "/dev/ttyAMA0");

    std::cout << "Config: " << configPath << std::endl;
    auto configJson = getConfig(configPath);
    if (configJson.is_object() && !configJson.empty()) {
        std::cout << configJson.dump(4) << std::endl;
        if (configJson.contains("file_length_ms"))
            global::fileLengthMs = configJson["file_length_ms"].get<int>();
        if (configJson.contains("uart_port"))
            global::uartPort = configJson["uart_port"].get<std::string>();
    } else {
        configJson["file_length_ms"] = global::fileLengthMs;
        configJson["uart_port"]      = global::uartPort;
        std::ofstream f(configPath);
        f << configJson.dump(4);
        std::cout << "Created default config at " << configPath << std::endl;
    }
    std::cout << "UART: " << global::uartPort << std::endl;

    std::thread tzmq(clientThread);
    std::thread timu(murataThread);
    tzmq.join();
    timu.join();
}