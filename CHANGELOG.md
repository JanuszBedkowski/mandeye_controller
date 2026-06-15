# Changelog

## [0.8] - 2026-06-15

### New features
- **Tracy profiler support** — optional build with `MANDEYE_USE_TRACY=ON`; ZoneScoped and TracyPlot instrumentation in LAZ save and lidar buffer paths
- **System stats in `/json/status`** — CPU temperature, RAM (total/available/used) and swap usage reported from `/proc/meminfo` and sysfs
- **Hesai lidar support** — full integration of Hesai AT128 / OT128 via HesaiLidarSDK
- **Ouster lidar support** — complete client implementation with scan processing and IMU data
- **SICK lidar support** — added via lidar abstraction layer
- **OLED status display** — extra service for SSD1306-style OLED via I2C
- **GNSS USB service client** — external GNSS device connected over USB

### Fixes
- Fixed `save_duration_sec1` always returning `-1.0` in continuous scan path (return value was discarded)
- Fixed beep-on-save on CM5 direct mode
- Fixed Livox Mid-360 driver compatibility

### Infrastructure
- CI sanity build (Debian Bookworm x64) added
- clang-format-18 enforcement in CI
- USB pendrive format changed from FAT32 to exFAT

---

## [0.5] - 2024

Initial tagged release with Livox Mid-360 support, Raspberry Pi 4/5, PPS synchronization, and LAZ file saving.