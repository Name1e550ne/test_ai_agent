cmake_minimum_required(VERSION 3.22)

project(demo_daemon
    VERSION 0.1.0
    DESCRIPTION "Production-quality background daemon service for Linux Debian"
    LANGUAGES CXX
)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(ENABLE_SANITIZERS "Enable AddressSanitizer, UndefinedBehaviorSanitizer, ThreadSanitizer" OFF)
option(BUILD_TESTS "Build unit tests" OFF)
option(USE_TSAN "Use ThreadSanitizer instead of ASan/LSan" OFF)

# Compile options
add_compile_options(-Wall -Wextra -Wpedantic)

# Debug/Release specific flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0)
    add_definitions(-DDEBUG)
else()
    add_compile_options(-O2)
endif()

# Sanitizers
if(ENABLE_SANITIZERS)
    if(USE_TSAN)
        # ThreadSanitizer (incompatible with ASan)
        add_compile_options(-fsanitize=thread)
        add_link_options(-fsanitize=thread)
    else()
        # ASan + LSan + UBSan
        add_compile_options(-fsanitize=address,undefined,leak)
        add_link_options(-fsanitize=address,undefined,leak)
    endif()
endif()

# Find dependencies
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(WARNING "nlohmann_json not found. Please install nlohmann-json3-dev package.")
    message(WARNING "Trying to use FetchContent as fallback...")
    
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Subdirectories
add_subdirectory(src/core)
add_subdirectory(src/ipc)
add_subdirectory(src/commands)
add_subdirectory(src/tasks)
add_subdirectory(src/daemon)
add_subdirectory(src/cli)

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install rules
install(TARGETS demo_daemon demo_daemon_cli
    RUNTIME DESTINATION bin
)

install(DIRECTORY deploy/systemd/
    DESTINATION lib/systemd/system
    FILES_MATCHING PATTERN "*.service"
)

# CPack configuration
set(CPACK_PACKAGE_NAME "demo_daemon")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Background daemon service")
set(CPACK_PACKAGE_VENDOR "Demo Project")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
include(CPack)
