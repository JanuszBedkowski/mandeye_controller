cmake_minimum_required(VERSION 3.13)
project(mandeye_fake_pps)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_path(LIBSERIAL_INCLUDE_DIR SerialPort.h
        PATH_SUFFIXES include
        PATHS /usr /usr/local /usr/include/libserial)
find_library(LIBSERIAL_LIBRARY
        NAMES libserial.so
        PATH_SUFFIXES lib
        PATHS /usr/lib)
include_directories(${LIBSERIAL_INCLUDE_DIR})

add_executable(mandeye_fake_pps main.cpp)
target_link_libraries(mandeye_fake_pps ${LIBSERIAL_LIBRARY} gpiod pthread mandeye_extra_utils)

install(TARGETS mandeye_fake_pps
        RUNTIME DESTINATION /opt/mandeye/extras/)

install(FILES services/mandeye_fake_pps.service DESTINATION /usr/lib/systemd/system)