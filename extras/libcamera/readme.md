# Mandeye Controller Libcamera driver

This project provides a web-based interface for controlling and monitoring a camera using libcamera. The interface allows you to view the camera stream, download full-resolution images, view and edit camera configuration, and inspect photo metadata in real time.

## Features

- **Live Camera Stream:**
  - The main page displays a live stream from the camera.
- **Photo Metadata:**
  - Photo metadata (from `/photoMeta`) is automatically displayed and updated below the camera image.
  - You can also manually refresh the metadata using the "Show Photo Metadata" button.
- **Download Full Image:**
  - Download the latest full-resolution image via the "Download Full Image" button (calls `/photoFull`).
- **Camera Configuration:**
  - View and edit camera configuration in JSON format.
  - Use the "Get Config" and "Set Config" buttons to retrieve and update configuration via `/getConfig` and `/setConfig` endpoints.
- **Stream Control:**
  - Start and stop the live stream using the provided buttons.
## Prerequisites
```aiignore
sudo apt install libcamera-dev libpistache-dev libopencv-dev
```

Add device overlay to /boot/firmware/config.txt:
```aiignore
camera_auto_detect=0
dtoverlay=imx519,cam0
```

Next test cameras:
```aiignore
pi@lineassistpro:~ $ rpicam-still --list-cameras
Available cameras
-----------------
0 : imx519 [4656x3496 10-bit RGGB] (/base/axi/pcie@120000/rp1/i2c@88000/imx519@1a)
    Modes: 'SRGGB10_CSI2P' : 1280x720 [80.01 fps - (1048, 1042)/2560x1440 crop]
                             1920x1080 [60.05 fps - (408, 674)/3840x2160 crop]
                             2328x1748 [30.00 fps - (0, 0)/4656x3496 crop]
                             3840x2160 [18.00 fps - (408, 672)/3840x2160 crop]
                             4656x3496 [9.00 fps - (0, 0)/4656x3496 crop]

```
## Build Instructions
```
cd mandeye_controller/extras/libcamera
mkdir build
cd build
cmake ..
make
sudo make install
```

## Start service
```
sudo systemctl enable mandeye_libcamera_cam0.service 
sudo systemctl start mandeye_libcamera_cam0.service 
    
``` 

## Endpoints

- `/photo` - Returns the current camera image (used for live stream).
- `/photoFull` - Returns the latest full-resolution image for download.
- `/photoMeta` - Returns JSON metadata for the current photo.
- `/getConfig` - Returns the current camera configuration as JSON. Includes `width`/`height` (active resolution) and `resolutions` (discrete list of selectable modes: pipeline output sizes plus native sensor modes).
- `/setConfig` - Accepts a JSON payload to update the running camera configuration.
- `/saveConfig` - Accepts a JSON payload, writes it to the USB config file (`--config` path) and makes it the loaded config. Used by the web UI's "Save to USB" button.

## Config file

The service is started with `--config /media/usb/camN_config.json`. If the file is missing,
a default is written from the live camera dump. Recognised top-level keys:

- `disabled` (bool) - if true the process sleeps forever and does not open the camera.
- `rateMs` (uint) - minimum ms between delivered frames.
- `width` / `height` (uint) - requested stream resolution; `validate()` snaps it to a
  deliverable size and the chosen size is logged (`Resolution: requested ... -> validated ...`).
- `picamera` (object) - one entry per libcamera control. Scalars are plain numbers / bools.
  Array / rectangle controls take a JSON array: `"ColourGains": [r, b]`,
  `"FrameDurationLimits": [minUs, maxUs]` (microseconds), `"ScalerCrop": [x, y, w, h]`. Keys starting with
  `_` and `null` values are ignored. `NoiseReductionMode` defaults to `1` (Fast) when the
  file does not set it.

Ready-made profiles live next to this readme and are installed to
`/opt/mandeye/extras/libcamera/`:

- `config_indoor.json` - AE on, mains flicker corrected via `AeFlickerMode 1` /
  `AeFlickerPeriod 10000` (100 Hz light ripple).
- `config_indoor_flickerfree.json` - fixed `ExposureTime 5000` us (< one mains half-cycle),
  AE drives gain only.

They are **mutually exclusive** (FlickerManual quantises exposure to `AeFlickerPeriod/2`
steps, which conflicts with a pinned `ExposureTime`). Copy one to `/media/usb/cam0_config.json`
(and `cam1_config.json`) and `sudo systemctl restart mandeye_libcamera_cam0`.

## Resolution & FOV

The IMX519 sensor modes have different analog crops (`rpicam-still --list-cameras`):
`2328x1748` and `4656x3496` read the full array `(0,0)/4656x3496` (full field of view,
2x2 bin for the smaller); `1920x1080`, `3840x2160`, `1280x720` are cropped
(narrower FOV, non-zero crop origin). `SCALER_CROP` in `/photoMeta` reports the active
crop in sensor-array pixels. **If it is not `(0,0)/<full array>`, the effective focal
length and principal point differ from a full-array calibration** - any offline
LiDAR<->camera intrinsics must be re-derived per resolution, or pin the resolution to a
full-FOV mode.

## Usage

1. Build and run the server (see your main project documentation for details).
2. Open the web interface in your browser (typically served on port 8004).
3. Use the controls on the right to interact with the camera:
    - View the live stream and metadata.
    - Download the full image.
    - View and edit configuration.
    - Start/stop the stream as needed.
   
![img.png](docs/img.png)
## File Overview

- `index.html.h` - Contains the embedded HTML, CSS, and JavaScript for the web UI.
- `LibCameraWrapper.cpp/.h` - Camera control logic (not detailed here).
- `main.cpp` - Main server logic (not detailed here).
- `CMakeLists.txt` - Build configuration.

## Notes

- The web UI is responsive and should work in modern browsers.
- The metadata and image are always in sync, as metadata is fetched with each image update.
- For development, you may need to adjust API endpoint URLs or ports as appropriate.

---




