// CDR gold tests for PointCloud2 and Imu.
//
// Prerequisite:
//   source /opt/ros/humble/setup.bash
//   python3 gen_cdr_gold.py > cdr_gold_data.h
//
// Build: cmake --build build -j$(nproc)
// Run:   ./build/test_cdr

#include "cdr_gold_data.h"
#include "cdr_serializer.hpp"

#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <vector>

// ===========================================================================
// PointCloud2 test inputs (must match gen_cdr_gold.py)
// ===========================================================================

static constexpr int32_t  kPc2StampSec  = 100;
static constexpr uint32_t kPc2StampNsec = 1'000'000;
static constexpr uint64_t kPc2StampNs   = 100'001'000'000ULL;
static constexpr uint64_t kPc2T0Ns      = 100'000'000'000ULL;
static constexpr uint32_t kPointStep    = 28;

struct Pt { float x, y, z, intensity; uint16_t ring; uint64_t ts; };
static constexpr Pt kPts[] = {
    {1.0f, 2.0f, 3.0f, 100.0f, 0, 100'000'000'000ULL},
    {4.0f, 5.0f, 6.0f, 200.0f, 1, 100'001'000'000ULL},
};
static constexpr uint32_t kNPts = 2;

// ===========================================================================
// Imu test inputs (must match gen_cdr_gold.py)
// ===========================================================================

static constexpr int32_t  kImuStampSec  = 200;
static constexpr uint32_t kImuStampNsec = 250'000'000;
static constexpr uint64_t kImuStampNs   =
    static_cast<uint64_t>(kImuStampSec) * 1'000'000'000ULL + kImuStampNsec;

static constexpr double kGyro[3]  = {0.1, 0.2, 0.3};
static constexpr double kAccel[3] = {1.0, 2.0, 9.81};

// ===========================================================================
// Helpers
// ===========================================================================

#define CHECK(expr, msg) \
    if(!(expr)) { std::cerr << "  FAIL [" << label << "] " << msg << "\n"; ok = false; }

static bool deq(double a, double b) { return std::abs(a - b) < 1e-12; }

// ===========================================================================
// PointCloud2: decode CDR buffer and verify all fields
// ===========================================================================

static bool checkPc2Cdr(const uint8_t* buf, size_t size, const char* label)
{
    CdrReader r(buf, size);
    bool ok = true;

    const int32_t  sec = r.read_i32();
    const uint32_t nsec = r.read_u32();
    const std::string fid = r.read_string();
    CHECK(sec  == kPc2StampSec,  "stamp.sec  got=" << sec);
    CHECK(nsec == kPc2StampNsec, "stamp.nsec got=" << nsec);
    CHECK(fid  == "lidar",       "frame_id   got='" << fid << "'");

    const uint32_t h = r.read_u32();
    const uint32_t w = r.read_u32();
    CHECK(h == 1,     "height got=" << h);
    CHECK(w == kNPts, "width  got=" << w);

    const uint32_t nf = r.read_u32();
    CHECK(nf == 6, "nFields got=" << nf);
    for(uint32_t i = 0; i < nf; ++i)
    {
        r.read_string();
        r.read_u32();
        r.read_u8();
        r.read_u32();
    }

    r.read_bool();
    const uint32_t ps = r.read_u32();
    r.read_u32();
    CHECK(ps == kPointStep, "point_step got=" << ps);

    const uint32_t dataLen = r.read_u32();
    CHECK(dataLen == kNPts * kPointStep, "dataLen got=" << dataLen);
    const uint8_t* raw = r.read_raw(dataLen);
    CHECK(raw != nullptr, "data read_raw returned null");

    if(raw)
    {
        const double t0 = kPc2T0Ns * 1e-9;
        for(uint32_t i = 0; i < kNPts; ++i)
        {
            const uint8_t* p = raw + i * kPointStep;
            float x, y, z, intens; uint16_t ring; double rel;
            std::memcpy(&x,     p + 0,  4);
            std::memcpy(&y,     p + 4,  4);
            std::memcpy(&z,     p + 8,  4);
            std::memcpy(&intens, p + 12, 4);
            std::memcpy(&ring,  p + 16, 2);
            std::memcpy(&rel,   p + 20, 8);

            const double expRel = kPts[i].ts * 1e-9 - t0;
            CHECK(x     == kPts[i].x,         "pt[" << i << "].x     got=" << x);
            CHECK(y     == kPts[i].y,         "pt[" << i << "].y     got=" << y);
            CHECK(z     == kPts[i].z,         "pt[" << i << "].z     got=" << z);
            CHECK(intens== kPts[i].intensity, "pt[" << i << "].intens got=" << intens);
            CHECK(ring  == kPts[i].ring,      "pt[" << i << "].ring  got=" << ring);
            CHECK(deq(rel, expRel),           "pt[" << i << "].rel_t got=" << rel
                                              << " want=" << expRel);
        }
    }

    const bool dense = r.read_bool();
    CHECK(dense, "is_dense");
    CHECK(r.ok(), "reader overflow");
    return ok;
}

// ===========================================================================
// PointCloud2: serialize with CdrWriter (mirrors McapWriter.cpp)
// ===========================================================================

static std::vector<uint8_t> serializeTestPc2()
{
    const double t0 = kPc2T0Ns * 1e-9;
    CdrWriter w;

    w.write_i32(static_cast<int32_t>(kPc2StampNs / 1'000'000'000ULL));
    w.write_u32(static_cast<uint32_t>(kPc2StampNs % 1'000'000'000ULL));
    w.write_string("lidar");
    w.write_u32(1);
    w.write_u32(kNPts);

    struct FDef { const char* name; uint32_t offset; uint8_t datatype; };
    static constexpr FDef kFields[] = {
        {"x", 0, 7}, {"y", 4, 7}, {"z", 8, 7},
        {"intensity", 12, 7}, {"ring", 16, 4}, {"time", 20, 8},
    };
    w.write_u32(6);
    for(const auto& f : kFields)
    {
        w.write_string(f.name);
        w.write_u32(f.offset);
        w.write_u8(f.datatype);
        w.write_u32(1);
    }

    w.write_bool(false);
    w.write_u32(kPointStep);
    w.write_u32(kNPts * kPointStep);
    w.write_u32(kNPts * kPointStep);

    for(uint32_t i = 0; i < kNPts; ++i)
    {
        const auto& p = kPts[i];
        w.write_raw(&p.x, 4);
        w.write_raw(&p.y, 4);
        w.write_raw(&p.z, 4);
        w.write_raw(&p.intensity, 4);
        w.write_raw(&p.ring, 2);
        const uint16_t pad = 0;
        w.write_raw(&pad, 2);
        const double t = p.ts * 1e-9 - t0;
        w.write_raw(&t, 8);
    }

    w.write_bool(true);
    return w.data();
}

// ===========================================================================
// Imu: decode CDR buffer and verify all fields
// ===========================================================================

static bool checkImuCdr(const uint8_t* buf, size_t size, const char* label)
{
    CdrReader r(buf, size);
    bool ok = true;

    const int32_t  sec  = r.read_i32();
    const uint32_t nsec = r.read_u32();
    const std::string fid = r.read_string();
    CHECK(sec  == kImuStampSec,  "stamp.sec  got=" << sec);
    CHECK(nsec == kImuStampNsec, "stamp.nsec got=" << nsec);
    CHECK(fid  == "lidar",       "frame_id   got='" << fid << "'");

    // orientation quaternion (identity: 0,0,0,1)
    const double ox = r.read_f64(), oy = r.read_f64(),
                 oz = r.read_f64(), ow = r.read_f64();
    CHECK(deq(ox, 0.0), "orient.x got=" << ox);
    CHECK(deq(oy, 0.0), "orient.y got=" << oy);
    CHECK(deq(oz, 0.0), "orient.z got=" << oz);
    CHECK(deq(ow, 1.0), "orient.w got=" << ow);

    // orientation_covariance[9]: -1 in [0], zeros elsewhere
    const double oc0 = r.read_f64();
    CHECK(deq(oc0, -1.0), "orient_cov[0] got=" << oc0);
    for(int i = 1; i < 9; ++i) r.read_f64();

    // angular_velocity
    const double gx = r.read_f64(), gy = r.read_f64(), gz = r.read_f64();
    CHECK(deq(gx, kGyro[0]), "gyro.x got=" << gx);
    CHECK(deq(gy, kGyro[1]), "gyro.y got=" << gy);
    CHECK(deq(gz, kGyro[2]), "gyro.z got=" << gz);

    // angular_velocity_covariance[9]
    for(int i = 0; i < 9; ++i) r.read_f64();

    // linear_acceleration
    const double ax = r.read_f64(), ay = r.read_f64(), az = r.read_f64();
    CHECK(deq(ax, kAccel[0]), "accel.x got=" << ax);
    CHECK(deq(ay, kAccel[1]), "accel.y got=" << ay);
    CHECK(deq(az, kAccel[2]), "accel.z got=" << az);

    // linear_acceleration_covariance[9]
    for(int i = 0; i < 9; ++i) r.read_f64();

    CHECK(r.ok(), "reader overflow");
    return ok;
}

// ===========================================================================
// Imu: serialize with CdrWriter (mirrors McapWriter.cpp::serializeImu)
// ===========================================================================

static std::vector<uint8_t> serializeTestImu()
{
    CdrWriter w;

    w.write_i32(static_cast<int32_t>(kImuStampNs / 1'000'000'000ULL));
    w.write_u32(static_cast<uint32_t>(kImuStampNs % 1'000'000'000ULL));
    w.write_string("lidar");

    // orientation quaternion — identity
    w.write_f64(0.0); w.write_f64(0.0); w.write_f64(0.0); w.write_f64(1.0);

    // orientation_covariance[9]
    w.write_f64(-1.0);
    for(int i = 1; i < 9; ++i) w.write_f64(0.0);

    // angular_velocity
    w.write_f64(kGyro[0]); w.write_f64(kGyro[1]); w.write_f64(kGyro[2]);

    // angular_velocity_covariance[9]
    for(int i = 0; i < 9; ++i) w.write_f64(0.0);

    // linear_acceleration
    w.write_f64(kAccel[0]); w.write_f64(kAccel[1]); w.write_f64(kAccel[2]);

    // linear_acceleration_covariance[9]
    for(int i = 0; i < 9; ++i) w.write_f64(0.0);

    return w.data();
}

// ===========================================================================
// main
// ===========================================================================

static int run(const char* name, bool(*check)(const uint8_t*, size_t, const char*),
               const uint8_t* gold, size_t goldSz,
               std::vector<uint8_t>(*serialize)())
{
    int result = 0;
    std::cout << name << " — gold decode: ";
    if(!check(gold, goldSz, "gold")) { std::cout << "FAIL\n"; result = 1; }
    else std::cout << "PASS\n";

    auto ours = serialize();
    std::cout << name << " — our encode:  ";
    if(!check(ours.data(), ours.size(), "ours")) { std::cout << "FAIL\n"; result = 1; }
    else std::cout << "PASS\n";

    return result;
}

int main()
{
    int rc = 0;
    rc |= run("PointCloud2", checkPc2Cdr, kGoldPc2, kGoldPc2Size, serializeTestPc2);
    rc |= run("Imu",         checkImuCdr, kGoldImu, kGoldImuSize, serializeTestImu);
    return rc;
}
