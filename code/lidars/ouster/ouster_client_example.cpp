/**
 * Copyright (c) 2022, Ouster, Inc.
 * All rights reserved.
 */

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include "../../../3rd/ouster-sdk/ouster_client/include/ouster/client.h"
#include "../../../3rd/ouster-sdk/ouster_client/include/ouster/lidar_scan.h"
#include "../../../3rd/ouster-sdk/ouster_client/include/ouster/sensor_scan_source.h"
#include "../../../3rd/ouster-sdk/ouster_client/include/ouster/types.h"
#include "../../../cmake-build-debug/3rd/ouster-sdk/generated/ouster/impl/build.h"

#include <../../../3rd/ouster-sdk/ouster_client/include/ouster/impl/logging.h>

using namespace ouster;

const size_t N_SCANS = 500;

int main(int argc, char* argv[])
{
    // Limit ouster_client log statements to "info"
    sensor::init_logger("debug");

    std::cerr << "Ouster client example " << ouster::SDK_VERSION << std::endl;

    std::vector<ouster::sensor::Sensor> sensors;
    ouster::sensor::sensor_config config;
    config.udp_profile_lidar = ouster::sensor::UDPProfileLidar::PROFILE_LIDAR_LEGACY;
    config.timestamp_mode = ouster::sensor::TIME_FROM_INTERNAL_OSC;
    ouster::sensor::Sensor sen("192.168.200.99", config);
    sensors.push_back(sen);

    sensor::SensorClient m_client(sensors);
    auto infos = m_client.get_sensor_info();
    for (const auto& info : infos)
    {
        size_t w = info.format.columns_per_frame;
        size_t h = info.format.pixels_per_column;

        ouster::sensor::ColumnWindow column_window = info.format.column_window;

        std::cerr << "  Firmware version:  " << info.image_rev << "\n  Serial number:     " << info.sn
                  << "\n  Product line:      " << info.prod_line << "\n  Scan dimensions:   " << w << " x " << h
                  << "\n  Column window:     [" << column_window.first << ", " << column_window.second << "]" << std::endl;
    }

    bool run_thread_ = true;
    std::vector<LidarScanFieldTypes> fields_;
    std::vector<std::unique_ptr<LidarScan>> scans;
    std::vector<ScanBatcher> batchers;

    for (const auto& meta : m_client.get_sensor_info())
    {
        fields_.push_back(get_field_types(meta.format.udp_profile_lidar));
    }

    for (int i = 0; i < sensors.size(); i++)
    {
        std::cout << "Sensor fields:" << infos[i].sn << std::endl;
        for (const auto& f : fields_[i])
        {
            std::cout << f.name << std::endl;
        }
    }

    for (size_t i = 0; i < infos.size(); i++)
    {
        const auto& info = infos[i];
        batchers.push_back(ScanBatcher(info));
        size_t w = info.format.columns_per_frame;
        size_t h = info.format.pixels_per_column;
        scans.push_back(std::make_unique<LidarScan>(w, h, fields_[i].begin(), fields_[i].end()));
    }
    std::vector<XYZLut> luts;
    for (const auto& info : infos)
    {
        luts.push_back(ouster::make_xyz_lut(info, true /* if extrinsics should be used or not */));
    }


    while (run_thread_)
    {
        std::string file_base = "scan_";
        static int file_ind = 0;

        auto p = m_client.get_packet(0.05);
        if (p.type == sensor::ClientEvent::Packet && p.packet().type() == sensor::PacketType::Imu)
        {
            const auto& info = infos[p.source];
            const auto& ip = static_cast<sensor::ImuPacket&>(p.packet());
            auto result = ip.validate(info);

            std::cout << "Received IMU packet with timestamp: " << double(ip.accel_ts()) / 1e9 << std::endl;
        }
        if (p.type == sensor::ClientEvent::Packet && p.packet().type() == sensor::PacketType::Lidar)
        {
            using namespace sensor;

            const auto& info = infos[p.source];
            const auto& lp = static_cast<LidarPacket&>(p.packet());
            auto result = lp.validate(info);
            std::cout << "Received Lidar packet with timestamp: " << double(lp.col_timestamp(lp.nth_col(0))) / 1e9 << std::endl;
            // Add the packet to the batch
            if (batchers[p.source](lp, *scans[p.source]))
            {
                {
                    std::cout << "Batcher for source: " << p.source << " completed a scan." << std::endl;
                    const auto& scan = *scans[p.source];
                    // Now process the cloud and save it
                    // First compute a point cloud using the lookup table
                    auto cloud = ouster::cartesian(scan, luts[p.source]);

                    // channel fields can be queried as well
                    auto n_valid_first_returns = (scan.field<uint32_t>(sensor::ChanField::RANGE) != 0).count();

                    // LidarScan also provides access to header information such as
                    // status and timestamp
                    auto status = scan.status();
                    auto it = std::find_if(
                        status.data(),
                        status.data() + status.size(),
                        [](const uint32_t s)
                        {
                            return (s & 0x01);
                        }); // find first valid status
                    if (it != status.data() + status.size())
                    {
                        auto ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::nanoseconds(scan.timestamp()(it - status.data()))); // get corresponding timestamp

                        std::cerr << "  Frame no. " << scan.frame_id << " with " << n_valid_first_returns << " valid first returns at "
                                  << ts_ms.count() << " ms" << std::endl;
                    }

                    // Finally save the scan
                    std::string filename = file_base + std::to_string(file_ind++) + ".csv";
                    std::ofstream out;
                    out.open(filename);
                    out << std::fixed << std::setprecision(4);
                    auto tsRef = scan.timestamp();
                    auto statusRef = scan.status();
                    // write each point, filtering out points without returns
                    std::cout << "Cloud size : " << cloud.rows() << " ts size : " << tsRef.size() << " status size : " << statusRef.size()
                              << std::endl;
                    for (int i = 0; i < cloud.rows(); i++)
                    {
                        int column = i % scan.w;
                        double ts = double(tsRef[column]) / 1e9;
                        auto xyz = cloud.row(i);
                        if (!xyz.isApproxToConstant(0.0))
                            out << xyz(0) << ", " << xyz(1) << ", " << xyz(2) << ", " << ts << std::endl;
                    }

                    out.close();
                    std::cerr << "  Wrote " << filename << std::endl;
                }


                const size_t w = info.format.columns_per_frame;
                const size_t h = info.format.pixels_per_column;
                scans[p.source] =
                    std::make_unique<LidarScan>(w, h, fields_[p.source].begin(), fields_[p.source].end(), info.format.columns_per_packet);
            }
        }
    };

    return EXIT_SUCCESS;
}
