cmake_minimum_required(VERSION 3.13)
project(LibCameraHello)


cmake_minimum_required(VERSION 3.13)
project(libcamera_jpeg_example)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCAMERA REQUIRED libcamera)

find_package( OpenCV REQUIRED )
include_directories( ${OpenCV_INCLUDE_DIRS} )

include_directories(${LIBCAMERA_INCLUDE_DIRS} ${LIBJPEG_INCLUDE_DIRS})

find_package(PkgConfig)
pkg_check_modules(Pistache REQUIRED IMPORTED_TARGET libpistache)

pkg_check_modules(ZMQ REQUIRED libzmq)

add_executable(mandeye_libcamera main.cpp LibCameraWrapper.cpp LibCameraWrapper.h)
target_link_libraries(mandeye_libcamera  ${ZMQ_LIBRARIES} PkgConfig::Pistache ${LIBCAMERA_LIBRARIES} ${OpenCV_LIBS} nlohmann_json::nlohmann_json)

install(TARGETS mandeye_libcamera
        RUNTIME DESTINATION /opt/mandeye/extras/)

install(FILES services/mandeye_libcamera.service  DESTINATION /usr/lib/systemd/system)

# Packaging
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "mandeye-extras-libcamera")
set(MANDEYE_VERSION "1.0.0")
set(CPACK_PACKAGE_VERSION "${MANDEYE_VERSION}-${GIT_HASH}-${MANDEYE_HARDWARE_HEADER}-${SYSTEM_ARCH}")
set(CPACK_PACKAGE_CONTACT "Michał Pełka <michalpelka@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "rapidjson-dev,libserial-dev, libgpiod-dev, libzmq3-dev, opecv4.5, libpistache-dev, libcamera-dev, libjpeg-dev")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/services/postinst")
include(CPack)
