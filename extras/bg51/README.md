# BG51 driver


## Description
The driver that connects to the camera and listens for commands from the controller.

## Installation

Prerequisites:
```bash
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install python3-zmq python3-systemmd
```

```bash

```

Service installation:
Create a file `/usr/lib/systemd/system/mandeye_bg51.service` with content.
Note that you need to adjust your user's name:

```bash
[Unit]
Description=Mandeye_BG51
After=multi-user.target

[Service]
User=pi
StandardOutput=null
StandardError=null
ExecStartPre=/bin/sleep 20
ExecStart=python3 -u /opt/mandeye/extras/bg51/radiation_sensor_driver.py
Restart=always

[Install]
WantedBy=multi-user.target
```

Next reload daemons, enable and start the service`
```
sudo systemctl daemon-reload
sudo systemctl enable mandeye_bg51.service
sudo systemctl start mandeye_bg51.service
```
You can check status of the service with:
```bash
sudo systemctl status mandeye_bg51.service
```
