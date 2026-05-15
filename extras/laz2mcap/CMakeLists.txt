add_executable(laz2mcap laz2mcap.cpp)
target_include_directories(laz2mcap PRIVATE ${MANDEYE_DIR}/code)
target_link_libraries(laz2mcap PRIVATE laszip mcap nlohmann_json::nlohmann_json mcap_tools)
