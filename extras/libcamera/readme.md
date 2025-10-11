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
sudo apt install libcamera-dev libpistache-dev libopencv-core-dev
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
sudo systemctl enable mandeye_libcamera.service 
sudo systemctl start mandeye_libcamera.service 
    
``` 

## Endpoints

- `/photo` - Returns the current camera image (used for live stream).
- `/photoFull` - Returns the latest full-resolution image for download.
- `/photoMeta` - Returns JSON metadata for the current photo.
- `/getConfig` - Returns the current camera configuration as JSON.
- `/setConfig` - Accepts a JSON payload to update the camera configuration.

## Usage

1. Build and run the server (see your main project documentation for details).
2. Open the web interface in your browser (typically served on port 8004).
3. Use the controls on the right to interact with the camera:
    - View the live stream and metadata.
    - Download the full image.
    - View and edit configuration.
    - Start/stop the stream as needed.

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




