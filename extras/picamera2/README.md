# Picamera2 driver


## Description
The driver that connects to the camera and listens for commands from the controller.

## Installation

Prerequisites:
```bash
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install python3-zmq python3-systemmd
```

Service installation:
Create a file `/usr/lib/systemd/system/mandeye_picamera.service` with content.
Note that you need to adjust your user's name:

```bash
[Unit]
Description=Mandeye_Picamera2
After=multi-user.target

[Service]
User=mandeye
StandardOutput=null
StandardError=null
ExecStartPre=/bin/sleep 20
ExecStart=python3 /home/mandeye/mandeye_controller/extras/picamera2/camera.py 
Restart=always

[Install]
WantedBy=multi-user.target

```

Next reload daemons, enable and start the service`
```
sudo systemctl daemon-reload
sudo systemctl enable mandeye_picamera.service
sudo systemctl start mandeye_picamera.service
```
You can check status of the service with:
```bash
sudo systemctl status mandeye_picamera.service
```
