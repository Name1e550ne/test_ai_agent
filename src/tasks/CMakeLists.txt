# demo_daemon Tasks Library

add_library(demo_daemon_tasks STATIC)

target_sources(demo_daemon_tasks PRIVATE
    task_manager.cpp
    builtin/heartbeat_task.cpp
    builtin/sleep_task.cpp
)

target_include_directories(demo_daemon_tasks PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(demo_daemon_tasks PUBLIC
    demo_daemon_core
    pthread
)

target_compile_features(demo_daemon_tasks PUBLIC cxx_std_20)
