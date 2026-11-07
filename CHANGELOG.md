# Changelog
## [0.82] - 2026-11-07

### New features
- **Livox Mid-360L support** — Livox SDK2 patched to accept `dev_type` 41 and drive it with the Mid-360 protocol and `MID360` config section
- **Lidar data-flow check at startup** — the controller waits up to 60 s for point data and enters `LIDAR_ERROR` if none arrives; `data_flowing` added to the Livox status in `/json/status`
- **Livox point tag saved to LAZ** — the per-point Livox tag is written to the LAZ classification field (point format 1, 5 bits). Tools that interpret ASPRS classes may read these values as classes

### Fixes
- LAZ classification no longer contains uninitialized data (`LidarPoint` fields are now zero-initialized)
- Startup lidar wait no longer holds the lidar client lock for the whole wait

## [0.81] - 2026-09-14

- ** Extra GNSS ** - hotfix service name.


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