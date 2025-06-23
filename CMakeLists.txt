cmake_minimum_required(VERSION 3.0.0)
set(CMAKE_CXX_STANDARD 17)

project (mandeye_multisensor)

set(MANDEYE_HARDWARE_HEADER CACHE STRING "Mandeye hardware setting. Choose one header from 'code/hardware/mandeye-*.h")
#add_compile_definitions(MANDEYE_HARDWARE_HEADER="${MANDEYE_HARDWARE_HEADER}")
message(STATUS "Mandeye hardware version is : ${MANDEYE_HARDWARE_HEADER}")
set (MANDEYE_VERSION "0.7-dev")
message(STATUS "Mandeye version is : ${MANDEYE_VERSION}")

set(SYSTEM_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
message("pistache")
#pistache
set(PISTACHE_BUILD_TESTS OFF CACHE BOOL "Disable Pistache tests")
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



# extras
add_subdirectory(extras)

include_directories(code/)

set(MANDEYE_USE_OUSTER_SDK ON CACHE BOOL  "Use Ouster SDK ")
set(MANDEYE_USE_LIVOX_SDK2 ON CACHE BOOL "Use Livox SDK (HAP/Mid360)")

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

# GPIOD

find_library(GPIOD_LIBRARY NAMES libgpiod.so)
if(NOT GPIOD_LIBRARY)
    message(FATAL_ERROR "gpiod library not found. Install apt install libgpiod-dev")
endif()

# get git info
execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Generate the header file
configure_file(
        ${CMAKE_SOURCE_DIR}/code/compilation_constants.h.in
        ${CMAKE_BINARY_DIR}/compilation_constants.h
        @ONLY
)
# Include the generated file in the project
include_directories(${CMAKE_BINARY_DIR})
#executable
set(LIDAR_SOURCES
    code/lidars/dummy/ButterLidar.cpp
    code/lidars/LidarImplementation.cpp
)
set(LIDAR_LIBRARIES )

if (${MANDEYE_USE_OUSTER_SDK})
    # We need to separate the Ouster SDK from the main project to avoid conflicts with SPDLOG in LIVOX SDK2
    message("Ouster SDK")
    add_subdirectory(3rd/ouster-sdk)
    add_executable(ouster_client_example code/ouster_client_example.cpp)
    target_link_libraries(ouster_client_example PRIVATE OusterSDK::ouster_client)
    
    # Create isolated static library for Ouster integration
    add_library(ouster_lib
        code/lidars/ouster/OusterClient.cpp
    )
    target_link_libraries(ouster_lib PRIVATE OusterSDK::ouster_client)
    target_include_directories(ouster_lib PRIVATE 3rd/ouster-sdk/ouster_client/include)
    
    list(APPEND LIDAR_LIBRARIES ouster_lib)
    add_compile_definitions(MANDEYE_USE_OUSTER_SDK)
endif ()

if (${MANDEYE_USE_LIVOX_SDK2})
    message("Livox SDK2")
    # Livox SDK2

    add_subdirectory(3rd/Livox-SDK2)
    add_library(livox2_lib 
            code/lidars/livoxsdk2/LivoxClient.cpp
    )
    target_link_libraries(livox2_lib PRIVATE livox_lidar_sdk_static)
    target_include_directories(livox2_lib PRIVATE 3rd/Livox-SDK2/include)
    list(APPEND LIDAR_LIBRARIES livox2_lib)
    # add defines
    add_compile_definitions(MANDEYE_USE_LIVOX_SDK2)
endif()


add_executable(control_program code/main.cpp code/gnss.cpp code/web_page.h
        ${LIDAR_SOURCES}
        ${LIDAR_SOURCES}
        code/gpios.cpp code/FileSystemClient.cpp code/save_laz.cpp
        code/utils/TimeStampReceiver.cpp code/publisher.cpp)

set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -latomic " )


target_link_libraries(control_program pthread ${LIDAR_LIBRARIES} pistache_static atomic laszip ${LIBSERIAL_LIBRARY} minea gpiod zmq)


add_executable(led_demo code/led_demo.cpp code/gpios.cpp)
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
target_link_libraries(led_demo gpiod pthread)

add_executable(fake_pps code/fake_pps.cpp code/gpios.cpp)
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
target_link_libraries(fake_pps ${LIBSERIAL_LIBRARY} gpiod pthread)


add_executable(button_demo code/button_demo.cpp code/gpios.cpp)
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
target_link_libraries(button_demo gpiod pthread)

# Define install directories
install(TARGETS control_program led_demo fake_pps button_demo
        RUNTIME DESTINATION /opt/mandeye/)

install(FILES packing/helpers.sh DESTINATION /opt/mandeye/)
install(FILES packing/helpers.sh DESTINATION /opt/mandeye/)
install(FILES packing/services/mandeye_controller.service  DESTINATION /usr/lib/systemd/system)
install(FILES packing/services/mandeye_fake_pps.service  DESTINATION /usr/lib/systemd/system)

# Packaging
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "mandeye-multisensor")
set(CPACK_PACKAGE_VERSION "${MANDEYE_VERSION}-${GIT_HASH}-${MANDEYE_HARDWARE_HEADER}-${SYSTEM_ARCH}")
set(CPACK_PACKAGE_CONTACT "Michał Pełka <michalpelka@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "rapidjson-dev,libserial-dev, libgpiod-dev, libzmq3-dev")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/packing/postinst/postinst")
include(CPack)