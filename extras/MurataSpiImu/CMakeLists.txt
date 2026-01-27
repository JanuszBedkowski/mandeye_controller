cmake_minimum_required(VERSION 3.13)
project(mandeye_extra_murata_spi_imu CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig)
pkg_check_modules(Pistache REQUIRED IMPORTED_TARGET libpistache)

pkg_check_modules(ZMQ REQUIRED libzmq)

add_library(murata_spi STATIC SCH1.c hw_spi_dev.c)

add_executable(murataDriver murataDriver.cpp)
target_link_libraries(murataDriver murata_spi  ${ZMQ_LIBRARIES} PkgConfig::Pistache)