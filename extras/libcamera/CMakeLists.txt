cmake_minimum_required(VERSION 3.13)
project(LibCameraHello)


cmake_minimum_required(VERSION 3.13)
project(libcamera_jpeg_example)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCAMERA REQUIRED libcamera)
pkg_check_modules(LIBJPEG REQUIRED libjpeg)

find_package( OpenCV REQUIRED )
include_directories( ${OpenCV_INCLUDE_DIRS} )

include_directories(${LIBCAMERA_INCLUDE_DIRS} ${LIBJPEG_INCLUDE_DIRS})
link_directories(${LIBCAMERA_LIBRARY_DIRS} ${LIBJPEG_LIBRARY_DIRS})

find_package(PkgConfig)
pkg_check_modules(Pistache REQUIRED IMPORTED_TARGET libpistache)

find_package(nlohmann_json REQUIRED)
include_directories(../../3rd/json/)
pkg_check_modules(ZMQ REQUIRED libzmq)

add_executable(CameraDriver main.cpp LibCameraWrapper.cpp LibCameraWrapper.h)
target_link_libraries(CameraDriver  ${ZMQ_LIBRARIES} PkgConfig::Pistache ${LIBCAMERA_LIBRARIES} ${LIBJPEG_LIBRARIES} ${OpenCV_LIBS})
