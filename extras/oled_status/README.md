# OLED Status Display

Displays live Mandeye scan status on a 128×64 SSD1306 OLED over hardware I2C.

## What it shows

Subscribes to the ZeroMQ status socket and renders on each refresh:

| Line | Content |
|------|---------|
| 1 | Current mode + LAZ/CSV file counts |
| 2–3 | Per `CAMERA_N` directory — file count and total MB |
| 4–5 | Per `LEPTON_*` directory — file count and total MB (name trimmed) |

File groups are scanned from `continuousScanTarget` each display cycle:
- `.laz` files in root → lidar (LAZ)
- `.csv` files in root → IMU (CSV)
- `CAMERA_N/` subdirectories → one row each (`CAM_N`)
- `LEPTON_*/` subdirectories → one row each (`LEP_<first5>`)

## Hardware

- Display: SSD1306 128×64 OLED, I2C address `0x3C`
- Bus: I2C bus 1 (default Raspberry Pi hardware I2C)
- Font: `u8g2_font_5x7_tr`

### Wiring

| OLED pin | Raspberry Pi pin |
|----------|-----------------|
| VCC | 3.3 V (pin 1) |
| GND | GND (pin 6) |
| SDA | GPIO 2 / SDA1 (pin 3) |
| SCL | GPIO 3 / SCL1 (pin 5) |

Enable hardware I2C in `/boot/firmware/config.txt`:

```
dtparam=i2c_arm=on
```

## Build

Built as part of the main CMake tree. Binary output: `oled_status`.

```bash
cmake -B build -S .
cmake --build build --target oled_status
```

## Install and start the service

```bash
sudo cp services/mandeye_oled_status.service /usr/lib/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable mandeye_oled_status
sudo systemctl start  mandeye_oled_status
```

## Check status

```bash
sudo systemctl status mandeye_oled_status
sudo journalctl -fu mandeye_oled_status
```