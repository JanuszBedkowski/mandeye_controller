#!/bin/bash
# Starts mandeye_libcamera instances in screen sessions.
# Each screen session runs an infinite restart loop.

BINARY=/opt/mandeye/extras/mandeye_libcamera
RETRY_DELAY=5

screen -dmS libcamera_cam0 bash -c "while true; do $BINARY --camera 0 --port 8004 --prefix cam0_ --config /media/usb/cam0_config.json; echo 'cam0 exited, restarting in ${RETRY_DELAY}s...'; sleep $RETRY_DELAY; done"
sleep 10
screen -dmS libcamera_cam1 bash -c "while true; do $BINARY --camera 1 --port 8005 --prefix cam1_ --config /media/usb/cam1_config.json; echo 'cam1 exited, restarting in ${RETRY_DELAY}s...'; sleep $RETRY_DELAY; done"

echo "Started libcamera_cam0 and libcamera_cam1 in screen sessions."
screen -ls