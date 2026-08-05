# demo_daemon Commands Library

add_library(demo_daemon_commands STATIC)

target_sources(demo_daemon_commands PRIVATE
    command_registry.cpp
    builtin/ping_command.cpp
    builtin/status_command.cpp
    builtin/shutdown_command.cpp
    builtin/commands_list_command.cpp
    builtin/tasks_commands.cpp
)

target_include_directories(demo_daemon_commands PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(demo_daemon_commands PUBLIC
    demo_daemon_core
    demo_daemon_tasks
    pthread
)

target_compile_features(demo_daemon_commands PUBLIC cxx_std_20)
