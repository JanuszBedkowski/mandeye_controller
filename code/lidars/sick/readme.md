# Build sick_scan_xd SDK without ROS 1 support


Clone and build the `sick_scan_xd` SDK.
```bash
```shell
git clone https://github.com/michalpelka/sick_scan_xd.git -b mp/fix_candidate_504
mkdir -p sick_scan_xd/build
cd sick_scan_xd/build
cmake -DROS_VERSION=0 -DLDMRS=0  -DBUILD_DEBUG_TARGET=OFF ..
make -j
```

Install the SDK

```bash
sudo make install
```
```
-- Installing: /usr/local/lib/libsick_scan_xd_shared_lib.so
-- Up-to-date: /usr/local/include/sick_scan_xd/sick_scan_api.h
-- Up-to-date: /usr/local/include/sick_scan_xd/sick_scan_api.py
-- Installing: /usr/local/lib/cmake/sick_scan_xd/sick_scan_xd-config.cmake
-- Installing: /usr/local/lib/cmake/sick_scan_xd/sick_scan_xd-config-debug.cmake
```

