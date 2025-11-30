# mandeye_controller

# Hardware documementation

## Supported hardware
We support the following hardware:
- Raspbery Pi 4 or 5 without any special hardware (bunch of of-the-shelf components and 3D printed parts)
- Raspbery Pi 4 Compute module with custom carrier board. The carrier board is not part of this repository and is available on request.

Both version shares the same software, but are different in hardware (e.g. different GPIO layout, some extra features on custom carrier board).
The target hardware is defined during compilation time.
If you want to extend this project to different platforms or hardware, please refer to source code available in [hardware](./code) directory.


## 3D Model

The source design is available on Onshape, and you can access it [here](https://cad.onshape.com/documents/a6c6019ccb399ad39d830fad/w/0440af658555626c8fea1136/e/e1bf637894052d9247ac984d?renderMode=0&uiState=651c527c78128a19b84b4be1).

The design files for 3D printing are available in the repository's 3mf directory. You can find the file [ScannerAssembly.3mf](./3mf/ScannerAssembly.3mf) containing the models to be printed.

For optimal results, we recommend printing the assembly with PLA. Don't forget to adjust the 3MF file according to your printer specifications. If you need assistance with this, consider including a brief guide or tips on printer adjustments.

![Scanner Assembly](./3mf/ScannerAssembly.png)

Additionally, we recommend printing a protective cap for Livox Mid-360, which you can find in the [LivoxCap.3mf](./3mf/LivoxCap.3mf) file.

![Livox Cap](./3mf/LivoxCap.png)

Feel free to reach out if you have any questions or need further assistance!

## Bill of Materials
[Click here to BIM](doc/BIM.md)

## Wiring

[Wiring manual](doc/wiring/wiring.md)


## Video guide 
Time-lapse video guide to build and configure the system.

[Youtube video](https://youtu.be/BXBbuSJMFEo)

> Note: the video will get outdated eventually, please refer to the manual.

# Software

# Important notes
Currently, Raspbian Bookworm is recommended for the system. The system is tested on Raspbian Bookworm. 
The version v0.5 supported Bullseye, the current main branch supports Bookworm.
The software can run other distros (e.g. Ubuntu), but it is not tested and maintained.

## Update system
```bash
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install build-essential cmake git rapidjson-dev debhelper build-essential ntfs-3g libserial-dev libgpiod-dev libzmq3-dev libpistache-dev libcamera-dev linuxptp debhelper libopencv-dev
```

## Static IP for eth0

```bash
sudo nano /etc/dhcpcd.conf
```
And add in the end:
```
interface eth0
static ip_address=192.168.1.5/24
static routers=0.0.0.0    
```

## Clone and build app

Note that, target hardware is defined during compilation time using `-MANDEYE_HARDWARE_HEADER=mandeye-standard-rpi4.h` parameter.
The `MANDEYE_HARDWARE_HEADER` is one of the headers in `code/hardware_config`:
```bash
$ ls ./code/hardware_config/mandeye-*.h
./code/hardware_config/mandeye-pro-cm4-bookworm.h
./code/hardware_config/mandeye-pro-cm4-bullseye.h
./code/hardware_config/mandeye-standard-rpi4.h
./code/hardware_config/mandeye-standard-rpi5.h
...
```

| Hardware header            | What is supported                                |
|----------------------------|--------------------------------------------------|
| mandeye-standard-rpi4.h    | Raspberry Pi 4                                   |
| mandeye-standard-rpi5.h    | Raspberry Pi 5                                   |
| mandeye-pro-cm4-bookworm.h | Raspberry Pi Compute module 4 plus carrier board |
| mandeye-direct-cm5.h       | Raspberry Pi Compute module 5 plus carrier board |
|mandeye-pro-cm4-bullseye.h  | Deprecated                                       |


The next example shows how to build the app for Raspberry Pi 4 without a custom carrier board at Raspberry Pi 4.
```
git clone https://github.com/JanuszBedkowski/mandeye_controller.git
cd mandeye_controller
git submodule init
git submodule update
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DMANDEYE_HARDWARE_HEADER=mandeye-standard-rpi4.h 
make -j1 # -j4 can be used for 8 Gb version
```

## Test wiring and hardware
Before running the application, you should test the wiring and hardware. You can use the following commands to test the hardware:
```bash
cd mandeye_controller/build
./led_demo
./button_demo
```
Both programs should work correctly, without a need of `sudo`. If not, check if your user is in `gpio` group, if not add it:
```bash
sudo usermod -a -G gpio $USER
```

## Set USB mount
Follow manual for build and configuration of USB mount
https://gist.github.com/michalpelka/82d44a21c29f34ee5320c349f8bbf683

```shell
cd /tmp
git clone https://github.com/rbrito/usbmount.git
cd usbmount
dpkg-buildpackage -us -uc -b
cd ..
sudo apt install ./usbmount_0.0.24_all.deb
```

Edit config `sudo nano /etc/usbmount/usbmount.conf`:
by changing keys:
```shell
FILESYSTEMS="vfat ext2 ext3 ext4 hfsplus ntfs fuseblk exfat"
FS_MOUNTOPTIONS="-fstype=vfat,users,rw,umask=000 -fstype=exfat,users,rw,umask=000"
VERBOSE=yes
```
Install udev rules:
```shell
sudo mkdir /etc/systemd/system/systemd-udevd.service.d
sudo nano -w /etc/systemd/system/systemd-udevd.service.d/00-my-custom-mountflags.conf
```
and add content:
```shell
[Service]
PrivateMounts=no
```

Restart udev:
```shell
sudo systemctl daemon-reexec
sudo service systemd-udevd restart
```

Verify if usb mount works correclty:
```shell
touch /media/usb/test
```
```shell
tail -f /var/log/syslog
```


# Installing software on target device

  Then you can build the package:
```bash
cd mandeye_controller
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DMANDEYE_HARDWARE_HEADER=mandeye-standard-rpi4.h
make -j1 # -j4 can be used for 8 Gb version
sudo make install 
```

## Debian package

You can build a DEB package for easier installation on the target device. 
It can help with deployment on small 1Gb devices.

```bash
cd mandeye_controller
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DMANDEYE_HARDWARE_HEADER=mandeye-standard-rpi4.h
make -j1 # -j4 can be used for 8 Gb version
sudo make package
```

The package will be available in the build directory.

To install the package, you need to copy it to the target device and install it with `dpkg`:
```bash
sudo dpkg -i mandeye_controller-0.1.0-Linux.deb
```

The package will be installed in `/opt/mandeye/` directory, the service will be added to `/usr/lib/systemd/system/`.
Note that the service will not start automatically, you need to start it manually or enable it with `systemctl`.
There is a helper script `/opt/mandeye/helper.sh` that contains some useful commands to start, stop, and restart the service.

If you make any changes post-installation, please run 'sudo systemctl daemon-reload' to reload the service.
Note that the changes will be disregarded after reinstallation.
## Post installation
Those commands need to be run only once after installation.


Add it to bashrc:
```bash
echo "source /opt/mandeye/helpers.sh" >> ~/.bashrc
```

After that start required services:

```shell
mandeye_start
```

There are optional services that can be started:

PPS time synchronization generator using GPIO:
```shell
sudo systemctl enable mandeye_fakepps.service
mandeye_fakepps_start # PPS generator using GPIO
```

PTP time synchronization generator using eth0 (works only on Raspberry Pi 5):
```shell
sudo systemctl enable mandeye_ptp4l-gm-eth0.service
sudo systemctl enable mandeye_phc2sys-gm-eth0.service 
mandeye_phc2sys_start # PTP clock using eth0
mandeye_ptp4l_grandmaster_start # 
```

Cameras:
```shell
sudo systemctl enable mandeye_libcamera_cam0.service
sudo systemctl enable mandeye_libcamera_cam1.service
mandeye_cam0_start
mandeye_cam1_start
```

Extra USB Gnss:
```shell
sudo systemctl enable mandeye_extra_gnss.service 
mandeye_extra_gnss_start
```

List state of services:
```shell
mandeye_services_status
```
