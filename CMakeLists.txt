cmake_minimum_required(VERSION 3.0.0)
set(CMAKE_CXX_STANDARD 17)

project (mandeye_multisensor)

option(MANEYE_HARWARE_STANDARD "Standard version using Raspberry pi 4" OFF)
option(MANEYE_HARWARE_PRO "Pro version using Compute module" OFF)

if(MANDEYE_HARDWARE_STANDARD)
    add_definitions(-DMANDEYE_HARDWARE_STANDARD)
endif()

if(MANDEYE_HARDWARE_PRO)
    add_definitions(-DMANDEYE_HARDWARE_PRO)
endif()

message("pistache")
#pistache
add_subdirectory(3rd/pistache)


message("LASzip library")
# LASzip library
add_subdirectory(3rd/LASzip)
include_directories(3rd/LASzip/include)

message("INCBIN")
# INCBIN
include_directories(3rd/incbin)

message("JSON")
#JSON
include_directories(3rd/json)

message("Livox SDK2")
# Livox SDK2
add_subdirectory(3rd/Livox-SDK2)

include_directories(code/)

# GPIOs
add_subdirectory(3rd/CppGPIO)
include_directories(3rd/CppGPIO/include/)

# NMEA and libserial
find_path(LIBSERIAL_INCLUDE_DIR SerialPort.h
        PATH_SUFFIXES include
        PATHS /usr /usr/local /usr/include/libserial)
message(${LIBSERIAL_INCLUDE_DIR})
find_library(LIBSERIAL_LIBRARY
        NAMES libserial.so
        PATH_SUFFIXES lib
        PATHS /usr/lib)
include_directories(3rd/minmea ${LIBSERIAL_INCLUDE_DIR})
add_library(minea STATIC 3rd/minmea/minmea.c)

#executable
add_executable(control_program code/main.cpp code/gnss.cpp code/web_page.h code/LivoxClient.cpp code/gpios.cpp code/FileSystemClient.cpp code/save_laz.cpp ${PISTACHE_SRC}
        code/utils/TimeStampReceiver.cpp)
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -latomic")
target_link_libraries(control_program pthread livox_lidar_sdk_static cppgpio-static pistache atomic laszip ${LIBSERIAL_LIBRARY} minea)
#target_link_libraries(control_program pthread livox_lidar_sdk_static cppgpio-static atomic laszip)


add_executable(led_demo code/led_demo.cpp code/gpios.cpp)
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
target_link_libraries(led_demo cppgpio-static)

add_executable(fake_pps code/fake_pps.cpp)
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
target_link_libraries(fake_pps cppgpio-static ${LIBSERIAL_LIBRARY})


add_executable(button_demo code/button_demo.cpp code/gpios.cpp)
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
target_link_libraries(button_demo cppgpio-static)
