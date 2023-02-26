cmake_minimum_required(VERSION 3.0.0)

project (mandeye_multisensor)

# mongoose library
include_directories(3rd/mongoose)
add_library(mongoose STATIC 3rd/mongoose/mongoose.c)

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

#executable
add_executable(control_program code/main.cpp code/web_server.cpp code/web_page.h code/LivoxClient.cpp)

target_link_libraries(control_program mongoose pthread livox_lidar_sdk_static)