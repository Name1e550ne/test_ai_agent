# demo_daemon_cli Executable

add_executable(demo_daemon_cli
    main.cpp
    client.cpp
)

target_link_libraries(demo_daemon_cli PRIVATE
    demo_daemon_core
    nlohmann_json::nlohmann_json
)

target_compile_features(demo_daemon_cli PRIVATE cxx_std_20)

# Install target
install(TARGETS demo_daemon_cli
    RUNTIME DESTINATION bin
)
