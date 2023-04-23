#!/bin/bash

cp /home/mandeye/mandeye_controller/service/mandeye_controllel.service /lib/systemd/system \
&& ls /lib/systemd/system/mandeye_controllel.service \
&& sudo chmod +x /lib/systemd/system/mandeye_controllel.service

systemctl daemon-reload \
&& systemctl enable mandeye_controllel.service \
&& systemctl stop mandeye_controllel.service \
&& systemctl start mandeye_controllel.service \
&& systemctl restart mandeye_controllel.service \
&& systemctl status mandeye_controllel.service
