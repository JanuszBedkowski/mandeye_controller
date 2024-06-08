# Setup additional UARTS

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