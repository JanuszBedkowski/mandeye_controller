# Prerequisites
All setup for Mandeye Pro board is done in the same way as for Mandeye board.

:bulb: **Note** You need to specify the correct board in the CMake configuration file. For Mandeye Pro board you need to set the `MAND_PRO` variable to `ON`:
```shell
git clone https://github.com/JanuszBedkowski/mandeye_controller.git
cd mandeye_controller
git submodule init
git submodule update
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DMANDEYE_HARDWARE_PRO:BOOL=ON
make -j4
```
# Building Project


Carrier boards supports number of extra serial ports. To enable them you need to add the following lines to the `/boot/config.txt` file:

```shell
[all]
enable_uart=1
dtoverlay=uart2
dtoverlay=uart3
dtoverlay=uart4
dtoverlay=uart5
```

After that you need to reboot the system.

Mappings of the UARTs to the physical pins are as follows:
 - '/dev/ttyS0' - UART0 - GPIO14, GPIO15, not used
 - '/dev/ttyAMA1' - hardware sync for Livox
 - '/dev/ttyAMA2' - GNSS
 - '/dev/ttyAMA3' - hardware sync for Livox

# Setup synchronization service for Livox

Create a file `/usr/lib/systemd/system/mandeye_fakepps.service` with content.
Note that you need to adjust your user's name:
```
[Unit]
Description=MandeyeFakePPS
After=multi-user.target

[Service]
User=mandeye
ExecStartPre=/bin/sleep 20
ExecStart=/home/mandeye/mandeye_controller/build/fake_pps
Restart=always

[Install]
WantedBy=multi-user.target
```

Next reload daemons, enable and start the service`
```
sudo systemctl daemon-reload
sudo systemctl enable mandeye_fakepps.service
sudo systemctl start mandeye_fakepps.service
```
You can check status of the service with:
```bash
sudo systemctl status mandeye_fakepps.service
```

## Ublox
The following config file is used for the Ublox GNSS module, use [U-Center2](https://www.u-blox.com/en/u-center-2) to configure the module.
Config: [mandeye_pro.ucf, for version 24.02, EXTCore 1.00 8d3640 firmware](mandeye_pro.ucf)
## Useful aliases
Add to your `~/.bashrc` file:
```bash
alias mandeye_start="sudo systemctl start mandeye_controller.service"
alias mandeye_stop="sudo systemctl stop mandeye_controller.service"
alias mandeye_status="sudo systemctl status mandeye_controller.service"
alias mandeye_log="journalctl -u mandeye.service"
alias mandeye_fakepps_start="sudo systemctl start mandeye_fakepps.service"
alias mandeye_fakepps_stop="sudo systemctl stop mandeye_fakepps.service"
alias mandeye_fakepps_status="sudo systemctl status mandeye_fakepps.service"
alias mandeye_fakepps_log="journalctl -u mandeye_fakepps.service"
alias GPIO_BUZZER_ON="raspi-gpio set 24 op && raspi-gpio set 24 dh"
alias GPIO_BUZZER_OFF="raspi-gpio set 24 dl"
alias mandeye_test="curl 127.0.0.1:8005/json/status --silent"
alias mandeye_gnss_sattelites="mandeye_test | grep satellites_tracked"
alias mandeye_gnss_data="stty -F /dev/ttyAMA2 38400 cs8 -cstopb -parenb && cat /dev/ttyAMA2"
alias mandeye_gnss_data_speed="stty -F /dev/ttyAMA2 38400 cs8 -cstopb -parenb && cat /dev/ttyAMA2 | pv -r > /dev/null"
```
