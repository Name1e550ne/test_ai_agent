# demo_daemon

Production-quality background daemon service for Linux Debian with IPC via Unix Domain Socket and JSON protocol.

## Features

- **IPC via Unix Domain Socket**: Newline-delimited JSON protocol
- **Extensible Command System**: Register custom commands easily
- **Background Task Manager**: Lifecycle management for background tasks
- **CLI Client**: Command-line interface for interaction
- **systemd Integration**: Ready-to-use systemd unit file
- **C++20**: Modern C++ with concepts, jthread, stop_token
- **Thread-safe**: Proper synchronization, no data races
- **Memory-safe**: RAII, smart pointers, no raw new/delete

## Requirements

- Debian 12 (Bookworm) or newer
- g++ 12+ with C++20 support
- CMake 3.22+
- nlohmann-json3-dev package

## Build

```bash
# Install dependencies
sudo apt install build-essential cmake libnlohmann-json3-dev

# Configure (Debug)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug

# Configure (Release)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# With sanitizers
cmake -B build -S . -DENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# With tests
cmake -B build -S . -DBUILD_TESTS=ON
cmake --build build
```

## Run

### Daemon

```bash
# Foreground mode (for systemd)
./build/src/daemon/demo_daemon --foreground

# Custom socket path
./build/src/daemon/demo_daemon --socket-path /tmp/demo.sock

# Log level
./build/src/daemon/demo_daemon --log-level debug
```

### CLI

```bash
# Ping
./build/src/cli/demo_daemon_cli ping

# Status
./build/src/cli/demo_daemon_cli status

# List commands
./build/src/cli/demo_daemon_cli commands list

# Tasks
./build/src/cli/demo_daemon_cli tasks list
./build/src/cli/demo_daemon_cli tasks add --type heartbeat --interval 5
./build/src/cli/demo_daemon_cli tasks stop --id <task_id>

# Shutdown
./build/src/cli/demo_daemon_cli shutdown
```

## Project Structure

```
demo_daemon/
├── CMakeLists.txt          # Root CMake configuration
├── include/demo_daemon/    # Public headers
│   ├── core/               # Core utilities (logger, errors)
│   ├── ipc/                # IPC layer (socket server, sessions)
│   ├── commands/           # Command system
│   └── tasks/              # Task management
├── src/
│   ├── core/               # Core implementation
│   ├── ipc/                # IPC implementation
│   ├── commands/           # Built-in commands
│   ├── tasks/              # Built-in tasks
│   ├── daemon/             # Daemon executable
│   └── cli/                # CLI executable
├── tests/                  # Unit tests
├── deploy/systemd/         # systemd unit files
└── docs/                   # Documentation
```

## IPC Protocol

Newline-delimited JSON over Unix Domain Socket.

**Request:**
```json
{"id": 1, "method": "ping", "params": {}}
```

**Response:**
```json
{"id": 1, "result": {"status": "ok"}}
```

**Error:**
```json
{"id": 1, "error": {"code": -32601, "message": "Method not found"}}
```

See [docs/PROTOCOL.md](docs/PROTOCOL.md) for details.

## Adding Commands

```cpp
class MyCommand : public ICommand {
public:
    std::string name() const override { return "my.command"; }
    std::string description() const override { return "Does something"; }
    nlohmann::json execute(const ServiceContext& ctx, const nlohmann::json& params) override {
        return {{"result", "done"}};
    }
};

// Register in main.cpp
registry.registerCommand(std::make_unique<MyCommand>());
```

See [docs/EXTENDING.md](docs/EXTENDING.md) for details.

## Adding Tasks

```cpp
class MyTask : public ITask {
public:
    void start(std::stop_token stop_token) override {
        while (!stop_token.stop_requested()) {
            // do work
        }
    }
    // ... other methods
};
```

## Testing

```bash
# Run tests
ctest --test-dir build

# Smoke test
./scripts/smoke_test.sh

# Sanitizers
cmake -B build -S . -DENABLE_SANITIZERS=ON
cmake --build build
./build/src/daemon/demo_daemon --foreground
```

## License

MIT License

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make changes
4. Add tests
5. Submit PR
