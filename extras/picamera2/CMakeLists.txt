install(FILES camera.py DESTINATION /opt/mandeye/extras/picamera2/)
install(FILES ftp_config_server.py DESTINATION /opt/mandeye/extras/picamera2/)
install(FILES services/mandeye_picamera.service  DESTINATION /usr/lib/systemd/system)
install(FILES services/mandeye_picamera_ftp.service  DESTINATION /usr/lib/systemd/system)

