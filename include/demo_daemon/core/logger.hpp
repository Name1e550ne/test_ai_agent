#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <mutex>
#include <memory>
#include <chrono>
#include <iomanip>
#include <atomic>
#include "demo_daemon/core/types.hpp"

namespace demo_daemon {

/// Потокобезопасный логгер с поддержкой уровней
class Logger {
public:
    explicit Logger(LogLevel minLevel = LogLevel::Info);

    // Запрет копирования
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Разрешение перемещения
    Logger(Logger&&) = default;
    Logger& operator=(Logger&&) = default;

    ~Logger() = default;

    /// Установка минимального уровня логирования
    void setMinLevel(LogLevel level);

    /// Получение минимального уровня
    [[nodiscard]] LogLevel minLevel() const noexcept;

    /// Логирование сообщения
    void log(LogLevel level, const std::string& message);

    /// Логирование с форматированием (простая версия)
    template<typename... Args>
    void log(LogLevel level, const std::string& format, Args&&... args);

    /// Convenience методы для каждого уровня
    void trace(const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

    template<typename... Args>
    void trace(const std::string& format, Args&&... args);
    
    template<typename... Args>
    void debug(const std::string& format, Args&&... args);
    
    template<typename... Args>
    void info(const std::string& format, Args&&... args);
    
    template<typename... Args>
    void warn(const std::string& format, Args&&... args);
    
    template<typename... Args>
    void error(const std::string& format, Args&&... args);

    /// Получить singleton logger
    static Logger& instance();

private:
    /// Форматирование префикса лога
    [[nodiscard]] std::string formatPrefix(LogLevel level) const;

    /// Запись в output
    void write(const std::string& formattedMessage);

    mutable std::mutex mutex_;
    std::atomic<LogLevel> minLevel_;
    std::ostream* output_;
    bool ownsOutput_;
};

// Inline реализации шаблонных методов

template<typename... Args>
void Logger::log(LogLevel level, const std::string& format, Args&&... args) {
    if (level >= minLevel()) {
        std::ostringstream oss;
        oss << "[DEMON] " << format;
        
        // Простая реализация без std::format (C++20)
        // Для production лучше использовать fmt library или std::format если доступен
        ((oss << " " << std::forward<Args>(args)), ...);
        
        log(level, oss.str());
    }
}

template<typename... Args>
void Logger::trace(const std::string& format, Args&&... args) {
    log(LogLevel::Trace, format, std::forward<Args>(args)...);
}

template<typename... Args>
void Logger::debug(const std::string& format, Args&&... args) {
    log(LogLevel::Debug, format, std::forward<Args>(args)...);
}

template<typename... Args>
void Logger::info(const std::string& format, Args&&... args) {
    log(LogLevel::Info, format, std::forward<Args>(args)...);
}

template<typename... Args>
void Logger::warn(const std::string& format, Args&&... args) {
    log(LogLevel::Warn, format, std::forward<Args>(args)...);
}

template<typename... Args>
void Logger::error(const std::string& format, Args&&... args) {
    log(LogLevel::Error, format, std::forward<Args>(args)...);
}

// Макросы для удобного логирования
#define LOG_TRACE(msg) ::demo_daemon::Logger::instance().trace(msg)
#define LOG_DEBUG(msg) ::demo_daemon::Logger::instance().debug(msg)
#define LOG_INFO(msg)  ::demo_daemon::Logger::instance().info(msg)
#define LOG_WARN(msg)  ::demo_daemon::Logger::instance().warn(msg)
#define LOG_ERROR(msg) ::demo_daemon::Logger::instance().error(msg)

#define LOG_TRACE_FMT(fmt, ...) ::demo_daemon::Logger::instance().trace(fmt, ##__VA_ARGS__)
#define LOG_DEBUG_FMT(fmt, ...) ::demo_daemon::Logger::instance().debug(fmt, ##__VA_ARGS__)
#define LOG_INFO_FMT(fmt, ...)  ::demo_daemon::Logger::instance().info(fmt, ##__VA_ARGS__)
#define LOG_WARN_FMT(fmt, ...)  ::demo_daemon::Logger::instance().warn(fmt, ##__VA_ARGS__)
#define LOG_ERROR_FMT(fmt, ...) ::demo_daemon::Logger::instance().error(fmt, ##__VA_ARGS__)

}  // namespace demo_daemon
