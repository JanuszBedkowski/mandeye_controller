#!/bin/sh

alias mandeye_start="sudo systemctl start mandeye_controller.service"
alias mandeye_stop="sudo systemctl stop mandeye_controller.service"
alias mandeye_status="sudo systemctl status mandeye_controller.service"
alias mandeye_log="journalctl -u mandeye.service"
alias mandeye_fakepps_start="sudo systemctl start mandeye_fakepps.service"
alias mandeye_fakepps_stop="sudo systemctl stop mandeye_fakepps.service"
alias mandeye_fakepps_status="sudo systemctl status mandeye_fakepps.service"
alias mandeye_fakepps_log="journalctl -u mandeye_fakepps.service"
alias mandeye_camera_start="sudo systemctl start mandeye_picamera.service"
alias mandeye_camera_stop="sudo systemctl stop mandeye_picamera.service"
alias mandeye_camera_status="sudo systemctl status mandeye_picamera.service"
alias mandeye_camera_ftp_start="sudo systemctl start mandeye_picamera_ftp.service"
alias mandeye_camera_ftp_stop="sudo systemctl stop mandeye_picamera_ftp.service"
alias mandeye_camera_ftp_status="sudo systemctl status mandeye_picamera_ftp.service"
alias GPIO_BUZZER_ON="raspi-gpio set 24 op && raspi-gpio set 24 dh"
alias GPIO_BUZZER_OFF="raspi-gpio set 24 dl"
alias mandeye_test="curl 127.0.0.1:8003/json/status --silent"
alias mandeye_gnss_sattelites="mandeye_test | grep satellites_tracked"
