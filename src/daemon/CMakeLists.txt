# demo_daemon Executable

add_executable(demo_daemon
    main.cpp
)

target_link_libraries(demo_daemon PRIVATE
    demo_daemon_core
    demo_daemon_ipc
    demo_daemon_commands
    demo_daemon_tasks
)

target_compile_features(demo_daemon PRIVATE cxx_std_20)

# Install target
install(TARGETS demo_daemon
    RUNTIME DESTINATION bin
)
