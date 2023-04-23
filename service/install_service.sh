#!/bin/bash

cp mandeye_controller.service /lib/systemd/system \
&& cp start_mandeye_controller.sh /usr/local/bin \
&& ls /lib/systemd/system/mandeye_controller.service \
&& chmod 777 /lib/systemd/system/mandeye_controller.service

systemctl daemon-reload \
&& systemctl enable mandeye_controller.service \
&& systemctl stop mandeye_controller.service \
&& systemctl start mandeye_controller.service \
&& systemctl restart mandeye_controller.service \
&& systemctl status mandeye_controller.service
