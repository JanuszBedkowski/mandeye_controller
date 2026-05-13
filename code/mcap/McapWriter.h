#pragma once
#include "lidars/BaseLidarClient.h"
#include <filesystem>
#include <memory>
#include <string>

// Forward-declare mcap types so this header stays light.
namespace mcap
{
class McapWriter;
}

namespace mandeye
{

// Writes LidarPoint and LidarIMU data to an MCAP file using ros2msg schema
// encoding + CDR serialization.  The resulting file is playable with
// `ros2 bag play` and renderable in Foxglove Studio without any ROS2 runtime.
//
// Topics:
//   /lidar_points  — sensor_msgs/msg/PointCloud2  (28 bytes/point)
//   /imu           — sensor_msgs/msg/Imu
//
// PointCloud2 field layout per point (point_step = 28):
//   x         float32  offset  0
//   y         float32  offset  4
//   z         float32  offset  8
//   intensity  float32  offset 12
//   ring       uint16   offset 16
//   time       float64  offset 20
class McapFileWriter
{
public:
    explicit McapFileWriter(const std::filesystem::path& path,
                            const std::string& frame_id = "lidar");
    ~McapFileWriter();

    // Non-copyable, movable
    McapFileWriter(const McapFileWriter&) = delete;
    McapFileWriter& operator=(const McapFileWriter&) = delete;

    // Write a batch of points.  timestamp_ns is the message publish time.
    void writePointCloud(uint64_t timestamp_ns, const LidarPointsBuffer& points);

    // Write a batch of IMU samples.  Each sample carries its own timestamp.
    void writeImu(uint64_t timestamp_ns, const LidarIMUBuffer& imu);

    // Write a single IMU sample.
    void writeImuSample(const LidarIMU& imu);

    bool isOpen() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mandeye
