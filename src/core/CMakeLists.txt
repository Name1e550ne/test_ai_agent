# demo_daemon Core Library

add_library(demo_daemon_core STATIC)

target_sources(demo_daemon_core PRIVATE
    logger.cpp
    error_codes.cpp
)

target_include_directories(demo_daemon_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(demo_daemon_core PUBLIC
    nlohmann_json::nlohmann_json
    pthread
)

target_compile_features(demo_daemon_core PUBLIC cxx_std_20)
