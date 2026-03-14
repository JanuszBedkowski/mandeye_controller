project(mandeye_extra_utils)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(mandeye_extra_utils STATIC ExtrasUtils.cpp)
target_include_directories(mandeye_extra_utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mandeye_extra_utils PUBLIC ${ZMQ_LIBRARIES})