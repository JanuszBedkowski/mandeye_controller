cmake_minimum_required(VERSION 3.13)
project(mandeye_slave_pps)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(../../3rd/minmea)

add_executable(slave_pps main.cpp ../../3rd/minmea/minmea.c)

install(TARGETS slave_pps
        RUNTIME DESTINATION /opt/mandeye/extras/)

install(FILES services/mandeye_slave_pps.service DESTINATION /usr/lib/systemd/system)
