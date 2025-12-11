#!/bin/bash
services=(
  mandeye_controller.service
  mandeye_fakepps.service
  mandeye_libcamera_cam0.service
  mandeye_libcamera_cam1.service
  mandeye_ptp4l-gm-eth0.service
  mandeye_phc2sys-gm-eth0.service
  mandeye_extra_gnss.service
)

echo "Checking Mandeye services..."
for s in "${services[@]}"; do
  echo -e "\n--- $s ---"
  systemctl is-active --quiet "$s" && echo "✅ Active" || echo "❌ Inactive"
done
