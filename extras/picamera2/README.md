# Picamera2 driver


## Description
The driver that connects to the camera and listens for commands from the controller.

## Installation - Camera service

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
User=pi
StandardOutput=null
StandardError=null
ExecStartPre=/bin/sleep 20
ExecStart=python3 /home/pi/mandeye_controller/extras/picamera2/camera.py 
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

## Config
The camera can be configured in runtime loading file from pendrive.
Place a file `/media/usb/mandeye/mandeye_config.json`:
```json
{
    "picamera":
    {
        "ExposureValue": -10,
        "AnalogueGain": 22
    }    
}
```
** Note ** service will create a default camera config `/media/usb/mandeye/mandeye_config.json.default` that User can take as template.


## Installation - FTP server for configuration
Create a file `/usr/lib/systemd/system/mandeye_picamera_ftp.service` with content.
Note that you need to adjust your user's name:

```bash
[Unit]
Description=Mandeye_Picamera2_FTP
After=multi-user.target

[Service]
User=pi
StandardOutput=null
StandardError=null
ExecStartPre=/bin/sleep 20
ExecStart=python3 /home/pi/mandeye_controller/extras/picamera2/ftp_config_server.py 
Restart=always

[Install]
WantedBy=multi-user.target
```

Next reload daemons, enable and start the service`
```
sudo systemctl daemon-reload
sudo systemctl enable mandeye_picamera_ftp.service
sudo systemctl start mandeye_picamera_ftp.service
```
You can check status of the service with:
```bash
sudo systemctl status mandeye_picamera_ftp.service
```



This service is hosting special FTP server that maps `/tmp/camera` dir to outside world at `ftp://raspber-pi-ip:2121`.

In this you have a copy of config file, that is watch. On file changed, it will trigger test photo.