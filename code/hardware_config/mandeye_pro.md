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
ExecStartPre=/bin/sleep 20
ExecStart=/home/robot/mandeye_controller/build/fake_pps
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
