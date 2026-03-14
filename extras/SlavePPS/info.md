# This tool allows to get PPS + NMEA signal from fake PPS.

Fake PPS is a service that will generate PPS signal at RPI output.
It is use usefull for generating PPS signal for LiDARS.
However, we can it sync slave Raspberry Pi.

# Prerequisites

1. Fake PPS running on master Raspberry Pi. You can follow the instructions in the README.md to set it up.
2. PPS enabled on slave Raspberry Pi

To enable PPS you need to add the following line to /boot/config.txt:
```aiignore
dtoverlay=pps-gpio,gpiopin=18
```
Test PPS with:
```aiignore
sudo ppstest /dev/pps0
```

3. Disable NTP on slave Raspberry Pi to prevent it from interfering with PPS signal:
```bash
sudo timedatectl set-ntp false
```