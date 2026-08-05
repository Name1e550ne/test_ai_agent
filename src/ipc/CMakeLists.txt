# demo_daemon IPC Library

add_library(demo_daemon_ipc STATIC)

target_sources(demo_daemon_ipc PRIVATE
    json_protocol.cpp
    unix_socket_server.cpp
    session_manager.cpp
    connection_manager.cpp
)

target_include_directories(demo_daemon_ipc PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(demo_daemon_ipc PUBLIC
    demo_daemon_core
    nlohmann_json::nlohmann_json
    pthread
)

target_compile_features(demo_daemon_ipc PUBLIC cxx_std_20)
