cmake_minimum_required(VERSION 3.13)
project(libcamera_jpeg_example)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



find_package( OpenCV REQUIRED )
include_directories( ${OpenCV_INCLUDE_DIRS} )

include_directories(${LIBCAMERA_INCLUDE_DIRS} ${LIBJPEG_INCLUDE_DIRS})

find_package(PkgConfig)
pkg_check_modules(Pistache REQUIRED IMPORTED_TARGET libpistache)

pkg_check_modules(ZMQ REQUIRED libzmq)


# NMEA and libserial
find_path(LIBSERIAL_INCLUDE_DIR SerialPort.h
        PATH_SUFFIXES include
        PATHS /usr /usr/local /usr/include/libserial)
message(${LIBSERIAL_INCLUDE_DIR})
find_library(LIBSERIAL_LIBRARY
        NAMES libserial.so
        PATH_SUFFIXES lib
        PATHS /usr/lib)
include_directories(../../3rd/minmea ${LIBSERIAL_INCLUDE_DIR})
include_directories(../../3rd/json)
add_library(mineaRtk STATIC ../../3rd/minmea/minmea.c)

add_executable(mandeye_rtkNmea main.cpp gnss.cpp ntrip.cpp)
MESSAGE("Libserial  ${LIBSERIAL_LIBRARY}")
target_link_libraries(mandeye_rtkNmea ${LIBSERIAL_LIBRARY} mineaRtk  ${ZMQ_LIBRARIES} PkgConfig::Pistache )

install(TARGETS mandeye_rtkNmea
        RUNTIME DESTINATION /opt/mandeye/extras/)
