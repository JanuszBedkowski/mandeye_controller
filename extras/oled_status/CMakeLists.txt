cmake_minimum_required(VERSION 3.10)
project("oled_status")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(U8G2_ARM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/u8g8-arm)

# Initialize c-periphery submodule if needed
if(NOT EXISTS "${U8G2_ARM_DIR}/c-periphery/src/gpio.c")
    message(STATUS "Initializing c-periphery submodule...")
    execute_process(
        COMMAND git submodule update --init
        WORKING_DIRECTORY ${U8G2_ARM_DIR}
        RESULT_VARIABLE GIT_RESULT
    )
    if(NOT GIT_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to initialize c-periphery submodule in ${U8G2_ARM_DIR}")
    endif()
endif()

# u8g2 C sources
file(GLOB U8G2_CSRC "${U8G2_ARM_DIR}/csrc/*.c")

# c-periphery sources (GPIO, I2C, SPI)
set(PERIPHERY_SRC
    ${U8G2_ARM_DIR}/c-periphery/src/gpio.c
    ${U8G2_ARM_DIR}/c-periphery/src/i2c.c
    ${U8G2_ARM_DIR}/c-periphery/src/spi.c
)

# Build u8g2 C core as a C-only static library.
# Keeping C sources isolated ensures the C compiler is always used, avoiding
# C++ parser issues with the raw binary font data in u8g2_fonts.c.
add_library(u8g2_csrc STATIC
    ${U8G2_CSRC}
    ${U8G2_ARM_DIR}/port/u8g2port.c
    ${PERIPHERY_SRC}
)

target_include_directories(u8g2_csrc PUBLIC
    ${U8G2_ARM_DIR}/csrc
    ${U8G2_ARM_DIR}/port
    ${U8G2_ARM_DIR}/c-periphery/src
)

target_compile_definitions(u8g2_csrc PRIVATE __ARM_LINUX__)


# u8g2 C++ wrapper — separate target so it links against the C core
add_library(u8g2_arm STATIC
    ${U8G2_ARM_DIR}/cppsrc/U8g2lib.cpp
    ${U8G2_ARM_DIR}/cppsrc/U8x8lib.cpp
    ${U8G2_ARM_DIR}/cppsrc/Print.cpp
)

target_link_libraries(u8g2_arm PUBLIC u8g2_csrc)

target_include_directories(u8g2_arm PUBLIC
    ${U8G2_ARM_DIR}/cppsrc
)

target_compile_definitions(u8g2_arm PRIVATE __ARM_LINUX__)

# oled_status executable
add_executable(mandeye_oled_status mandeye_oled_status.cpp)
target_link_libraries(mandeye_oled_status u8g2_arm m mandeye_extra_utils)
target_compile_definitions(mandeye_oled_status PRIVATE __ARM_LINUX__)
install(TARGETS mandeye_oled_status
        RUNTIME DESTINATION /opt/mandeye/extras/)