# Service to run gPTP / PTP on Livox 


# PTP Service

`/etc/systemd/system/mandeye_ptp4l-gm-eth0.service`

```
[Unit]
Description=linuxptp Grandmaster (ptp4l) on eth0
Wants=network-online.target
After=network-online.target sys-subsystem-net-devices-eth0.device
Requires=sys-subsystem-net-devices-eth0.device

[Service]
Type=simple
User=root
ExecStartPre=/sbin/ip link show dev eth0
ExecStart=/usr/sbin/ptp4l -f /etc/linuxptp/gm-l2.conf -i eth0 -m -2 --tx_timestamp_timeout 100
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target

```

and its config `/etc/linuxptp/gm-l2.conf`:
```
[global]
time_stamping         hardware
network_transport     L2    
delay_mechanism       E2E
domainNumber          0
twoStepFlag           1
summary_interval      1
logAnnounceInterval   1
logSyncInterval       0
summary_interval      1
```


# Master sync

`/etc/systemd/system/mandeye_phc2sys-gm-eth0.service `

```
[Unit]
Description=phc2sys (CLOCK_REALTIME -> /dev/ptp0) on eth0
After=mandeye_ptp4l-gm-eth0.service
Requires=mandeye_ptp4l-gm-eth0.service
[Service]
Type=simple
User=root
ExecStart=/usr/sbin/phc2sys -s CLOCK_REALTIME -c /dev/ptp0 -O 0 -w -m
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
```

# Using

start services:
```
sudo systemctl daemon-reload
sudo systemctl start mandeye_ptp4l-gm-eth0.service
sudo systemctl start mandeye_ptp4l-gm-eth0.service
```

# The result:
Run `mandeye_test` and enjoy:
```
        "LivoxLidarInfo": {
            "dev_type": 10,
            "lidar_ip": "192.168.1.100",
            "m_elapsed": 0,
            "m_elapsed_s": 0.0,
            "m_sessionStart": 0,
            "m_sessionStart_s": 0.0,
            "sn": "5CWD244K41094Q1",
            "timestamp": 1759785643503796783,
            "timestamp_s": 1759785643.5037968
        },

````
