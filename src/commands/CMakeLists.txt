# demo_daemon Commands Library

add_library(demo_daemon_commands STATIC)

target_sources(demo_daemon_commands PRIVATE
    ping_command.cpp
    shutdown_command.cpp
    commands_list_command.cpp
)

target_include_directories(demo_daemon_commands PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(demo_daemon_commands PUBLIC
    demo_daemon_core
    pthread
)

target_compile_features(demo_daemon_commands PUBLIC cxx_std_20)
