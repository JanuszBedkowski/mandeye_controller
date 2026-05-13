#define MCAP_IMPLEMENTATION
#include "McapWriter.h"
#include "cdr_serializer.hpp"
#include <iostream>
#include <mcap/mcap.hpp>  // writer + reader implementations compiled here once

namespace mandeye
{

// ---------------------------------------------------------------------------
// ros2msg schema strings (full definitions including nested types)
// ---------------------------------------------------------------------------

static constexpr const char* kPointCloud2Schema = R"(std_msgs/Header header
uint32 height
uint32 width
sensor_msgs/PointField[] fields
bool    is_bigendian
uint32  point_step
uint32  row_step
uint8[] data
bool    is_dense
================================================================================
MSG: std_msgs/Header
builtin_interfaces/Time stamp
string frame_id
================================================================================
MSG: builtin_interfaces/Time
int32 sec
uint32 nanosec
================================================================================
MSG: sensor_msgs/PointField
string name
uint32 offset
uint8  datatype
uint32 count
uint8 INT8=1
uint8 UINT8=2
uint8 INT16=3
uint8 UINT16=4
uint8 INT32=5
uint8 UINT32=6
uint8 FLOAT32=7
uint8 FLOAT64=8
)";

static constexpr const char* kImuSchema = R"(std_msgs/Header header
geometry_msgs/Quaternion orientation
float64[9] orientation_covariance
geometry_msgs/Vector3 angular_velocity
float64[9] angular_velocity_covariance
geometry_msgs/Vector3 linear_acceleration
float64[9] linear_acceleration_covariance
================================================================================
MSG: std_msgs/Header
builtin_interfaces/Time stamp
string frame_id
================================================================================
MSG: builtin_interfaces/Time
int32 sec
uint32 nanosec
================================================================================
MSG: geometry_msgs/Quaternion
float64 x
float64 y
float64 z
float64 w
================================================================================
MSG: geometry_msgs/Vector3
float64 x
float64 y
float64 z
)";

// ---------------------------------------------------------------------------
// PointCloud2 binary layout per point (28 bytes, little-endian)
// ---------------------------------------------------------------------------
//  offset  type     field
//       0  float32  x
//       4  float32  y
//       8  float32  z
//      12  float32  intensity
//      16  uint16   ring
//      18  (2-byte pad)
//      20  float64  time   (seconds relative to scan start)
//      28  --- end of point ---

static constexpr uint32_t kPointStep = 28;

// Write one point as 28 raw bytes in the agreed layout.
// Done field-by-field to avoid C++ struct padding surprises.
static void writePointBytes(CdrWriter& w, const LidarPoint& p, double t0)
{
    w.write_raw(&p.x, 4);
    w.write_raw(&p.y, 4);
    w.write_raw(&p.z, 4);
    w.write_raw(&p.intensity, 4);
    w.write_raw(&p.line_id, 2);
    const uint16_t pad = 0;
    w.write_raw(&pad, 2);
    const double t = p.timestamp * 1e-9 - t0;
    w.write_raw(&t, 8);
}

// ---------------------------------------------------------------------------
// CDR helpers
// ---------------------------------------------------------------------------

static void writeHeader(CdrWriter& w, uint64_t timestamp_ns, const std::string& frame_id)
{
    w.write_i32(static_cast<int32_t>(timestamp_ns / 1'000'000'000ULL));
    w.write_u32(static_cast<uint32_t>(timestamp_ns % 1'000'000'000ULL));
    w.write_string(frame_id);
}

static std::vector<uint8_t> serializePointCloud2(uint64_t timestamp_ns,
                                                  const LidarPointsBuffer& pts,
                                                  const std::string& frame_id)
{
    CdrWriter w;

    // Header
    writeHeader(w, timestamp_ns, frame_id);

    // height / width (unordered: height=1, width=N)
    w.write_u32(1);
    w.write_u32(static_cast<uint32_t>(pts.size()));

    // PointField[] — 6 fields
    struct FieldDef
    {
        const char* name;
        uint32_t offset;
        uint8_t datatype; // 7=FLOAT32, 4=UINT16, 8=FLOAT64
    };
    static constexpr FieldDef kFields[] = {
        {"x", 0, 7},
        {"y", 4, 7},
        {"z", 8, 7},
        {"intensity", 12, 7},
        {"ring", 16, 4},
        {"time", 20, 8},
    };
    w.write_u32(6); // sequence length
    for(const auto& f : kFields)
    {
        w.write_string(f.name);
        w.write_u32(f.offset);
        w.write_u8(f.datatype);
        w.write_u32(1); // count
    }

    w.write_bool(false);                                          // is_bigendian
    w.write_u32(kPointStep);                                      // point_step
    w.write_u32(static_cast<uint32_t>(pts.size()) * kPointStep); // row_step

    // data[] — uint32 length + raw bytes (no internal CDR padding)
    const uint32_t dataBytes = static_cast<uint32_t>(pts.size()) * kPointStep;
    w.write_u32(dataBytes);

    const double t0 = pts.empty() ? 0.0 : pts.front().timestamp * 1e-9;
    for(const auto& p : pts)
        writePointBytes(w, p, t0);

    w.write_bool(true); // is_dense
    return w.data();
}

static std::vector<uint8_t> serializeImu(uint64_t timestamp_ns,
                                          const LidarIMU& imu,
                                          const std::string& frame_id)
{
    CdrWriter w;
    writeHeader(w, timestamp_ns, frame_id);

    // orientation quaternion — identity (no orientation data from lidar IMU)
    w.write_f64(0.0); // x
    w.write_f64(0.0); // y
    w.write_f64(0.0); // z
    w.write_f64(1.0); // w

    // orientation_covariance[9] — -1 in [0] signals "unknown"
    w.write_f64(-1.0);
    for(int i = 1; i < 9; ++i)
        w.write_f64(0.0);

    // angular_velocity (rad/s)
    w.write_f64(static_cast<double>(imu.gyro_x));
    w.write_f64(static_cast<double>(imu.gyro_y));
    w.write_f64(static_cast<double>(imu.gyro_z));

    // angular_velocity_covariance[9]
    for(int i = 0; i < 9; ++i)
        w.write_f64(0.0);

    // linear_acceleration (m/s²)
    w.write_f64(static_cast<double>(imu.acc_x));
    w.write_f64(static_cast<double>(imu.acc_y));
    w.write_f64(static_cast<double>(imu.acc_z));

    // linear_acceleration_covariance[9]
    for(int i = 0; i < 9; ++i)
        w.write_f64(0.0);

    return w.data();
}

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct McapFileWriter::Impl
{
    mcap::McapWriter writer;
    mcap::ChannelId lidarChannelId{0};
    mcap::ChannelId imuChannelId{0};
    std::string frameId;
    bool open{false};
};

McapFileWriter::McapFileWriter(const std::filesystem::path& path, const std::string& frame_id)
    : impl_(std::make_unique<Impl>())
{
    impl_->frameId = frame_id;

    mcap::McapWriterOptions opts("ros2");
    opts.compression = mcap::Compression::None;
    opts.chunkSize = 128ULL * 1024 * 1024;

    auto status = impl_->writer.open(path.string(), opts);
    if(!status.ok())
    {
        std::cerr << "McapWriter: failed to open " << path << ": " << status.message << "\n";
        return;
    }

    // Register PointCloud2 schema
    mcap::Schema pc2Schema("sensor_msgs/msg/PointCloud2", "ros2msg",
                           {reinterpret_cast<const std::byte*>(kPointCloud2Schema),
                            reinterpret_cast<const std::byte*>(kPointCloud2Schema) +
                                std::strlen(kPointCloud2Schema)});
    impl_->writer.addSchema(pc2Schema);

    // Register Imu schema
    mcap::Schema imuSchema("sensor_msgs/msg/Imu", "ros2msg",
                           {reinterpret_cast<const std::byte*>(kImuSchema),
                            reinterpret_cast<const std::byte*>(kImuSchema) +
                                std::strlen(kImuSchema)});
    impl_->writer.addSchema(imuSchema);

    // Register channels
    mcap::Channel lidarChannel("/lidar_points", "cdr", pc2Schema.id);
    impl_->writer.addChannel(lidarChannel);
    impl_->lidarChannelId = lidarChannel.id;

    mcap::Channel imuChannel("/imu", "cdr", imuSchema.id);
    impl_->writer.addChannel(imuChannel);
    impl_->imuChannelId = imuChannel.id;

    impl_->open = true;
}

McapFileWriter::~McapFileWriter()
{
    if(impl_ && impl_->open)
        impl_->writer.close();
}

bool McapFileWriter::isOpen() const
{
    return impl_ && impl_->open;
}

void McapFileWriter::writePointCloud(uint64_t timestamp_ns, const LidarPointsBuffer& points)
{
    if(!isOpen() || points.empty())
        return;

    auto payload = serializePointCloud2(timestamp_ns, points, impl_->frameId);

    mcap::Message msg;
    msg.channelId = impl_->lidarChannelId;
    msg.sequence = 0;
    msg.publishTime = timestamp_ns;
    msg.logTime = timestamp_ns;
    msg.data = reinterpret_cast<const std::byte*>(payload.data());
    msg.dataSize = payload.size();

    auto status = impl_->writer.write(msg);
    if(!status.ok())
        std::cerr << "McapWriter: write error: " << status.message << "\n";
}

void McapFileWriter::writeImuSample(const LidarIMU& sample)
{
    if(!isOpen()) return;

    const uint64_t ts = sample.timestamp;
    auto payload = serializeImu(ts, sample, impl_->frameId);

    mcap::Message msg;
    msg.channelId   = impl_->imuChannelId;
    msg.sequence    = 0;
    msg.publishTime = ts;
    msg.logTime     = ts;
    msg.data        = reinterpret_cast<const std::byte*>(payload.data());
    msg.dataSize    = payload.size();

    auto s = impl_->writer.write(msg);
    if(!s.ok())
        std::cerr << "McapWriter: IMU write error: " << s.message << "\n";
}

void McapFileWriter::writeImu(uint64_t timestamp_ns, const LidarIMUBuffer& imu)
{
    for(const auto& sample : imu)
    {
        LidarIMU s = sample;
        if(s.timestamp == 0) s.timestamp = timestamp_ns;
        writeImuSample(s);
    }
}

} // namespace mandeye
