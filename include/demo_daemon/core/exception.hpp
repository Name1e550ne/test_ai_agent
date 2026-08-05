#pragma once

#include <stdexcept>
#include <string>
#include "demo_daemon/core/result.hpp"

namespace demo_daemon {

/// Базовый класс исключений демона
class DaemonException : public std::runtime_error {
public:
    explicit DaemonException(const std::string& message)
        : std::runtime_error(message) {}
    
    DaemonException(const std::string& message, const std::error_code& ec)
        : std::runtime_error(message + ": " + ec.message()) {}
};

/// Исключение для ошибок инициализации
class InitializationException : public DaemonException {
public:
    explicit InitializationException(const std::string& message)
        : DaemonException(message) {}
};

/// Исключение для ошибок конфигурации
class ConfigurationException : public DaemonException {
public:
    explicit ConfigurationException(const std::string& message)
        : DaemonException(message) {}
};

/// Исключение для ошибок IPC
class IpcException : public DaemonException {
public:
    explicit IpcException(const std::string& message)
        : DaemonException(message) {}
    
    IpcException(const std::string& message, const std::error_code& ec)
        : DaemonException(message, ec) {}
};

/// Исключение для ошибок протокола
class ProtocolException : public DaemonException {
public:
    explicit ProtocolException(const std::string& message)
        : DaemonException(message) {}
};

/// Исключение для ошибок команды
class CommandException : public DaemonException {
public:
    explicit CommandException(const std::string& message)
        : DaemonException(message) {}
};

/// Исключение для ошибок задачи
class TaskException : public DaemonException {
public:
    explicit TaskException(const std::string& message)
        : DaemonException(message) {}
};

/// Исключение для остановки (graceful shutdown)
class ShutdownException : public DaemonException {
public:
    explicit ShutdownException(const std::string& message = "Daemon shutdown requested")
        : DaemonException(message) {}
};

}  // namespace demo_daemon
