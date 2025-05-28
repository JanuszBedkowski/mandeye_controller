#include "OusterClient.h"

// OusterSDK includes - only in implementation file
#include "ouster/client.h"
#include "ouster/lidar_scan.h"
#include "ouster/types.h"
#include "ouster/impl/build.h"
#include "ouster/sensor_scan_source.h"
#include "ouster/impl/cartesian.h"
#include <ouster/impl/logging.h>

#include <chrono>
#include <iostream>
#include <deque>

namespace mandeye
{

// Implementation class that contains all OusterSDK-specific code
class OusterClientImpl
{
public:
    OusterClientImpl();
    ~OusterClientImpl();

    // Configuration
    nlohmann::json m_config;
    std::string m_sensorHostname{"192.168.200.99"};
    
    std::vector<ouster::sensor::Sensor> m_sensors;
    std::vector<ouster::sensor::sensor_info> m_sensorInfos;
    std::vector<ouster::LidarScanFieldTypes> m_fields;
    std::vector<std::unique_ptr<ouster::LidarScan>> m_scans;
    std::vector<ouster::ScanBatcher> m_batchers;
    std::vector<ouster::XYZLut> m_luts;

    // Threading
    std::thread m_dataThread;
    std::atomic<bool> m_isDone{false};
    
    // Data buffers
    mutable std::mutex m_lidarBufferMutex;
    mutable std::mutex m_imuBufferMutex;
    LidarPointsBufferPtr m_lidarBuffer{nullptr};
    LidarIMUBufferPtr m_imuBuffer{nullptr};
    
    // Timestamps and session tracking
    mutable std::mutex m_timestampMutex;
    uint64_t m_currentTimestamp{0};
    std::optional<uint64_t> m_sessionStart;
    uint64_t m_sessionElapsed{0};
    
    // Statistics
    mutable std::mutex m_statsMutex;
    std::unordered_map<uint32_t, uint64_t> m_receivedScans;
    std::unordered_map<uint32_t, uint64_t> m_droppedScans;
    
    // Sensor info mapping
    mutable std::mutex m_sensorInfoMutex;
    std::unordered_map<uint32_t, std::string> m_sensorIdToSerial;
    
    // Initialization
    bool m_initSuccess{false};
    
    // Private methods
    void dataThreadFunction();
    void processScan(const ouster::LidarScan& scan);
    void updateTimestamp(uint64_t timestamp);

};

OusterClientImpl::OusterClientImpl()
{
    ouster::sensor::init_logger("debug");
}

OusterClientImpl::~OusterClientImpl()
{
    m_isDone = true;
    if (m_dataThread.joinable()) {
        m_dataThread.join();
    }
}


OusterClient::OusterClient() : m_impl(std::make_unique<OusterClientImpl>())
{
}

OusterClient::~OusterClient()
{
    if (m_impl) {
        m_impl->m_isDone = true;
        if (m_impl->m_dataThread.joinable()) {
            m_impl->m_dataThread.join();
        }
    }
}

void OusterClient::Init(const nlohmann::json& config)
{
    m_impl->m_config = config;
    
    // Parse configuration
    if (config.contains("hostname")) {
        m_impl->m_sensorHostname = config["hostname"].get<std::string>();
    }
}

nlohmann::json OusterClient::produceStatus()
{
    nlohmann::json status;
    
    status["init_success"] = m_impl->m_initSuccess;
    status["hostname"] = m_impl->m_sensorHostname;
    //
    // Sensor information
    if (m_impl->m_initSuccess && !m_impl->m_sensorInfos.empty()) {
        status["sensors"] = nlohmann::json::array();
        for (size_t i = 0; i < m_impl->m_sensorInfos.size(); ++i) {
            nlohmann::json sensorStatus;
            sensorStatus["id"] = i;
            sensorStatus["serial"] = m_impl->m_sensorInfos[i].sn;
            sensorStatus["product_line"] = m_impl->m_sensorInfos[i].prod_line;
            sensorStatus["firmware_version"] = m_impl->m_sensorInfos[i].image_rev;
            sensorStatus["columns"] = m_impl->m_sensorInfos[i].format.columns_per_frame;
            sensorStatus["pixels_per_column"] = m_impl->m_sensorInfos[i].format.pixels_per_column;
            status["sensors"].push_back(sensorStatus);
        }
    }

    // Statistics
    {
        std::lock_guard<std::mutex> lock(m_impl->m_statsMutex);
        status["counters"]["received_scans"] = m_impl->m_receivedScans;
        status["counters"]["dropped_scans"] = m_impl->m_droppedScans;
    }

    // Timestamp information
    {
        std::lock_guard<std::mutex> lock(m_impl->m_timestampMutex);
        status["timestamp"] = m_impl->m_currentTimestamp;
        status["timestamp_s"] = double(m_impl->m_currentTimestamp) / 1e9;
        status["session_start"] = m_impl->m_sessionStart.value_or(-1);
        status["session_start_s"] = m_impl->m_sessionStart.has_value()
            ? double(*m_impl->m_sessionStart) / 1e9 : -1.0;
        status["session_elapsed"] = m_impl->m_sessionElapsed;
        status["session_elapsed_s"] = double(m_impl->m_sessionElapsed) / 1e9;
    }

    // Buffer status
    {
        std::lock_guard<std::mutex> lidarLock(m_impl->m_lidarBufferMutex);
        std::lock_guard<std::mutex> imuLock(m_impl->m_imuBufferMutex);

        if (m_impl->m_lidarBuffer) {
            status["buffers"]["lidar"]["count"] = m_impl->m_lidarBuffer->size();
        } else {
            status["buffers"]["lidar"]["count"] = "NULL";
        }

        if (m_impl->m_imuBuffer) {
            status["buffers"]["imu"]["count"] = m_impl->m_imuBuffer->size();
        } else {
            status["buffers"]["imu"]["count"] = "NULL";
        }
    }
    
    return status;
}

bool OusterClient::startListener(const std::string& interfaceIp)
{
    try {

        // Start data processing thread
        m_impl->m_isDone = false;
        m_impl->m_dataThread = std::thread(&OusterClientImpl::dataThreadFunction, m_impl.get());

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to start Ouster client: " << e.what() << std::endl;
        m_impl->m_initSuccess = false;
        return false;
    }
}

void OusterClient::startLog()
{
    assert(m_impl);
    std::lock_guard<std::mutex> lidarLock(m_impl->m_lidarBufferMutex);
    std::lock_guard<std::mutex> imuLock(m_impl->m_imuBufferMutex);
    
    m_impl->m_lidarBuffer = std::make_shared<LidarPointsBuffer>();
    m_impl->m_imuBuffer = std::make_shared<LidarIMUBuffer>();
    
    std::cout << "Started Ouster data logging" << std::endl;
}

void OusterClient::stopLog()
{
    assert(m_impl);
    std::lock_guard<std::mutex> lidarLock(m_impl->m_lidarBufferMutex);
    std::lock_guard<std::mutex> imuLock(m_impl->m_imuBufferMutex);
    
    m_impl->m_lidarBuffer = nullptr;
    m_impl->m_imuBuffer = nullptr;
    
    std::cout << "Stopped Ouster data logging" << std::endl;
}

std::pair<LidarPointsBufferPtr, LidarIMUBufferPtr> OusterClient::retrieveData()
{
    assert(m_impl);
    std::lock_guard<std::mutex> lidarLock(m_impl->m_lidarBufferMutex);
    std::lock_guard<std::mutex> imuLock(m_impl->m_imuBufferMutex);
    
    LidarPointsBufferPtr returnLidarBuffer = std::make_shared<LidarPointsBuffer>();
    LidarIMUBufferPtr returnImuBuffer = std::make_shared<LidarIMUBuffer>();
    
    std::swap(m_impl->m_lidarBuffer, returnLidarBuffer);
    std::swap(m_impl->m_imuBuffer, returnImuBuffer);
    
    return {returnLidarBuffer, returnImuBuffer};
}

std::unordered_map<uint32_t, std::string> OusterClient::getSerialNumberToLidarIdMapping() const
{
    assert(m_impl);
    std::lock_guard<std::mutex> lock(m_impl->m_sensorInfoMutex);
    return m_impl->m_sensorIdToSerial;
}

double OusterClient::getTimestamp()
{
    assert(m_impl);
    std::lock_guard<std::mutex> lock(m_impl->m_timestampMutex);
    return double(m_impl->m_currentTimestamp) / 1e9;
}

double OusterClient::getSessionDuration()
{
    assert(m_impl);
    std::lock_guard<std::mutex> lock(m_impl->m_timestampMutex);
    return double(m_impl->m_sessionElapsed) / 1e9;
}

double OusterClient::getSessionStart()
{
    assert(m_impl);
    std::lock_guard<std::mutex> lock(m_impl->m_timestampMutex);
    if (m_impl->m_sessionStart.has_value()) {
        return double(*m_impl->m_sessionStart) / 1e9;
    }
    return -1.0;
}

void OusterClient::initializeDuration()
{
    assert(m_impl);
    std::lock_guard<std::mutex> lock(m_impl->m_timestampMutex);
    if (!m_impl->m_sessionStart.has_value()) {
        m_impl->m_sessionStart = m_impl->m_currentTimestamp;
    }
}

void OusterClientImpl::dataThreadFunction()
{
    std::cout << "Ouster data thread started" << std::endl;
    ouster::sensor::init_logger("debug", "ouster.log");
    using ouster::sensor::logger;

    logger().info("Ouster client thread started");
    
    try {
        ouster::sensor::sensor_config config;
        config.udp_profile_lidar = ouster::sensor::UDPProfileLidar::PROFILE_LIDAR_LEGACY;
        config.timestamp_mode = ouster::sensor::TIME_FROM_INTERNAL_OSC;
        ouster::sensor::Sensor sen(m_sensorHostname, config);
        m_sensors.push_back(sen);

        ouster::sensor::SensorClient m_client(m_sensors);

        m_sensorInfos = m_client.get_sensor_info();
        
        // Initialize sensor info mapping
        {
            std::lock_guard<std::mutex> lock(m_sensorInfoMutex);
            for (size_t i = 0; i < m_sensorInfos.size(); ++i) {
                m_sensorIdToSerial[i] = m_sensorInfos[i].sn;
            }
        }
        
        for (const auto& info : m_sensorInfos)
        {
            size_t w = info.format.columns_per_frame;
            size_t h = info.format.pixels_per_column;

            ouster::sensor::ColumnWindow column_window = info.format.column_window;

            std::cerr << "  Firmware version:  " << info.image_rev << "\n  Serial number:     " << info.sn
                      << "\n  Product line:      " << info.prod_line << "\n  Scan dimensions:   " << w << " x " << h
                      << "\n  Column window:     [" << column_window.first << ", " << column_window.second << "]" << std::endl;
        }

        // Initialize fields, scans, batchers, and LUTs
        for (const auto& meta : m_sensorInfos)
        {
            m_fields.push_back(ouster::get_field_types(meta.format.udp_profile_lidar));
        }

        for (size_t i = 0; i < m_sensorInfos.size(); i++)
        {
            const auto& info = m_sensorInfos[i];
            m_batchers.push_back(ouster::ScanBatcher(info));
            size_t w = info.format.columns_per_frame;
            size_t h = info.format.pixels_per_column;
            m_scans.push_back(std::make_unique<ouster::LidarScan>(w, h, m_fields[i].begin(), m_fields[i].end()));
            m_luts.push_back(ouster::make_xyz_lut(info, true));
        }

        m_initSuccess = true;
        
        // Main data processing loop
        while (!m_isDone) {
            auto p = m_client.get_packet(0.05);
            
            if (p.type == ouster::sensor::ClientEvent::Packet && p.packet().type() == ouster::sensor::PacketType::Imu)
            {
                const auto& ip = static_cast<ouster::sensor::ImuPacket&>(p.packet());

                // Update timestamp from IMU
                updateTimestamp(ip.accel_ts());

                // Process IMU data if logging is active
                {
                    std::lock_guard<std::mutex> lock(m_imuBufferMutex);
                    if (m_imuBuffer) {
                        LidarIMU imuData{};
                        imuData.timestamp = ip.accel_ts();
                        imuData.acc_x = static_cast<float>(ip.la_x());
                        imuData.acc_y = static_cast<float>(ip.la_y());
                        imuData.acc_z = static_cast<float>(ip.la_z());
                        imuData.gyro_x = static_cast<float>(ip.av_x());
                        imuData.gyro_y = static_cast<float>(ip.av_y());
                        imuData.gyro_z = static_cast<float>(ip.av_z());
                        imuData.laser_id = static_cast<uint16_t>(p.source);
                        imuData.epoch_time = ip.accel_ts();
                        m_imuBuffer->emplace_back(imuData);
                    }
                }
            }
            
            if (p.type == ouster::sensor::ClientEvent::Packet && p.packet().type() == ouster::sensor::PacketType::Lidar)
            {
                const auto& info = m_sensorInfos[p.source];
                const auto& lp = static_cast<ouster::sensor::LidarPacket&>(p.packet());
                
                // Update timestamp from Lidar
                updateTimestamp(lp.col_timestamp(lp.nth_col(0)));
                
                // Add the packet to the batch
                if (m_batchers[p.source](lp, *m_scans[p.source]))
                {
                    {
                        std::lock_guard<std::mutex> lock(m_statsMutex);
                        m_receivedScans[p.source]++;
                    }
                    
                    processScan(*m_scans[p.source]);
                    
                    // Reset the scan for next batch
                    const size_t w = info.format.columns_per_frame;
                    const size_t h = info.format.pixels_per_column;
                    m_scans[p.source] = std::make_unique<ouster::LidarScan>(
                        w, h, m_fields[p.source].begin(), m_fields[p.source].end(), info.format.columns_per_packet);
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in Ouster data thread: " << e.what() << std::endl;
        m_initSuccess = false;
    }
    
    std::cout << "Ouster data thread stopped" << std::endl;
}

void OusterClientImpl::processScan(const ouster::LidarScan& scan)
{
    std::lock_guard<std::mutex> lock(m_lidarBufferMutex);
    if (!m_lidarBuffer) {
        return; // Not logging
    }

    // Find the source index for this scan
    size_t sourceIndex = 0; // For now, assume single sensor
    if (sourceIndex >= m_luts.size()) {
        return;
    }

    // Convert scan to point cloud
    auto cloud = ouster::cartesian(scan, m_luts[sourceIndex]);
    
    // Get timestamp data
    auto tsRef = scan.timestamp();
    auto statusRef = scan.status();
    
    // Process each point
    for (size_t i = 0; i < static_cast<size_t>(cloud.rows()); i++)
    {
        const int column = i % scan.w;
        auto xyz = cloud.row(i);
        
        // Skip points with no valid return
        if (xyz.isApproxToConstant(0.0)) {
            continue;
        }
        
        // Create lidar point
        LidarPoint point{};
        point.x = static_cast<float>(xyz(0));
        point.y = static_cast<float>(xyz(1));
        point.z = static_cast<float>(xyz(2));
        point.timestamp = tsRef[column];
        point.laser_id = static_cast<uint16_t>(sourceIndex);
        point.line_id = static_cast<uint8_t>(static_cast<size_t>(i) / scan.w); // Row index
        point.tag = 0;
        point.intensity = 0;
        
        // // Add additional fields if available
        // try {
        //     auto reflectivity = scan.field<uint32_t>(ouster::sensor::ChanField::REFLECTIVITY);
        //     point.intensity = static_cast<float>(reflectivity(i));
        // } catch (...) {
        //     // Field not available
        // }
        //
        try {
            auto range = scan.field<uint32_t>(ouster::sensor::ChanField::RANGE);
            // Range field is available but we don't have a field for it in LidarPoint
            // Could be used for validation if needed
        } catch (...) {
            // Field not available
        }
        
        m_lidarBuffer->push_back(point);
    }
}

void OusterClientImpl::updateTimestamp(uint64_t timestamp)
{
    std::lock_guard<std::mutex> lock(m_timestampMutex);
    
    m_currentTimestamp = timestamp;
    
    if (m_sessionStart.has_value()) {
        m_sessionElapsed = m_currentTimestamp - *m_sessionStart;
    }
}

} // namespace mandeye