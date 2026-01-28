cmake_minimum_required(VERSION 3.13)
project(mandeye_extra_murata_spi_imu CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig)
pkg_check_modules(Pistache REQUIRED IMPORTED_TARGET libpistache)

pkg_check_modules(ZMQ REQUIRED libzmq)

add_library(murata_spi STATIC SCH1.c hw_spi_dev.c)

add_executable(mandeye_murata_spi_driver murataDriver.cpp)
target_link_libraries(mandeye_murata_spi_driver murata_spi  ${ZMQ_LIBRARIES} PkgConfig::Pistache)

install(TARGETS mandeye_murata_spi_driver
        RUNTIME DESTINATION /opt/mandeye/extras/)

install(FILES services/mandeye_extra_murata_spi_imu.service  DESTINATION /usr/lib/systemd/system)
