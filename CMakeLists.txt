cmake_minimum_required(VERSION 3.0.0)
set(CMAKE_CXX_STANDARD 17)

project (mandeye_multisensor)


message("pistache")
#pistache
add_subdirectory(3rd/pistache)

#include_directories(3rd/pistache/include)
#set(PISTACHE_SRC 
#3rd/pistache/src/server/endpoint.cc
#3rd/pistache/src/server/listener.cc
#3rd/pistache/src/server/router.cc
#3rd/pistache/src/common/base64.cc       
##3rd/pistache/src/common/http_defs.cc     
#3rd/pistache/src/common/net.cc      
#3rd/pistache/src/common/stream.cc         
#3rd/pistache/src/common/transport.cc
#3rd/pistache/src/common/cookie.cc       
#3rd/pistache/src/common/http_header.cc   
#3rd/pistache/src/common/os.cc       
#3rd/pistache/src/common/string_logger.cc  
#3rd/pistache/src/common/utils.cc
#3rd/pistache/src/common/description.cc  
#3rd/pistache/src/common/http_headers.cc  
#3rd/pistache/src/common/peer.cc     
#3rd/pistache/src/common/tcp.cc
#3rd/pistache/src/common/http.cc         
#3rd/pistache/src/common/mime.cc          
#3rd/pistache/src/common/reactor.cc  
#3rd/pistache/src/common/timer_pool.cc
#)

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


#executable
add_executable(control_program code/main.cpp  code/web_page.h code/LivoxClient.cpp code/gpios.cpp code/FileSystemClient.cpp code/save_laz.cpp ${PISTACHE_SRC})
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -latomic")
target_link_libraries(control_program pthread livox_lidar_sdk_static cppgpio-static pistache atomic laszip)
#target_link_libraries(control_program pthread livox_lidar_sdk_static cppgpio-static atomic laszip)


add_executable(led_demo code/led_demo.cpp code/gpios.cpp)
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
target_link_libraries(led_demo cppgpio-static)

add_executable(button_demo code/button_demo.cpp code/gpios.cpp)
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
target_link_libraries(button_demo cppgpio-static)

install(TARGETS control_program DESTINATION bin)
install(TARGETS button_demo DESTINATION bin)
install(TARGETS led_demo DESTINATION bin)
