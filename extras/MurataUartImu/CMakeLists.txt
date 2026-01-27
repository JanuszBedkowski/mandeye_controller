cmake_minimum_required(VERSION 3.13)
project(mandeye_extra_murata_uart_imu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ      REQUIRED libzmq)
pkg_check_modules(SERIAL   REQUIRED libserial)

add_executable(mandeye_murata_uart_driver murataDriver.cpp)
target_include_directories(mandeye_murata_uart_driver PRIVATE ${ZMQ_INCLUDE_DIRS} ${SERIAL_INCLUDE_DIRS})
target_link_libraries(mandeye_murata_uart_driver ${ZMQ_LIBRARIES} ${SERIAL_LIBRARIES})

install(TARGETS mandeye_murata_uart_driver
        RUNTIME DESTINATION /opt/mandeye/extras/)

install(FILES services/mandeye_murata_uart_driver.service
        DESTINATION /usr/lib/systemd/system)