# mandeye_controller

# Hardware documementation

## 3D Model

The source design is available on Onshape at [this link](https://cad.onshape.com/documents/a6c6019ccb399ad39d830fad/w/0440af658555626c8fea1136/e/e1bf637894052d9247ac984d?renderMode=0&uiState=651c527c78128a19b84b4be1).
The design is available for download on Printables at [this link](https://www.printables.com/model/602273-open-source-3d-scanner).
The protective cap for the Livox 360 can be found on Printables at [this link](https://www.printables.com/pl/model/578608-protective-cap-for-livox-mid-306).

## Bill of Materials
[Click here to BIM](doc/BIM.md)

## Bill of Materials
[Wiring manual](doc/wiring/wiring.md)


# Software

## Update system
```bash
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install build-essential cmake git rapidjson-dev debhelper build-essential ntfs-3g
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

```
git clone https://github.com/JanuszBedkowski/mandeye_controller.git
cd mandeye_controller
git submodule init
git submodule update
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
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
FILESYSTEMS="vfat ext2 ext3 ext4 hfsplus ntfs fuseblk"
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
