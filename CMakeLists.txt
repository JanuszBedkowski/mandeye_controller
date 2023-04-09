cmake_minimum_required(VERSION 3.0.0)
set(CMAKE_CXX_STANDARD 17)

project (mandeye_multisensor)

## mongoose library
#include_directories(3rd/mongoose)
#add_library(mongoose STATIC 3rd/mongoose/mongoose.c)

#pistache
add_subdirectory(3rd/pistache)


# LASzip library
add_subdirectory(3rd/LASzip)
include_directories(3rd/LASzip/include)

# INCBIN
include_directories(3rd/incbin)

#JSON
include_directories(3rd/json)

# Livox SDK2
add_subdirectory(3rd/Livox-SDK2)

include_directories(code/)

# GPIOs
add_subdirectory(3rd/CppGPIO)
include_directories(3rd/CppGPIO/include/)


#executable
add_executable(control_program code/main.cpp  code/web_page.h code/LivoxClient.cpp code/gpios.cpp code/FileSystemClient.cpp code/save_laz.cpp)
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -latomic")
target_link_libraries(control_program pthread livox_lidar_sdk_static cppgpio-static pistache atomic laszip)

add_executable(led_demo code/led_demo.cpp code/gpios.cpp)
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
target_link_libraries(led_demo cppgpio-static)

add_executable(button_demo code/button_demo.cpp code/gpios.cpp)
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
target_link_libraries(button_demo cppgpio-static)