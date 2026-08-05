#include "demo_daemon/core/logger.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace demo_daemon {

Logger::Logger(LogLevel minLevel)
    : minLevel_(minLevel)
    , output_(&std::clog)
    , ownsOutput_(false) {}

void Logger::setMinLevel(LogLevel level) {
    minLevel_.store(level, std::memory_order_relaxed);
}

LogLevel Logger::minLevel() const noexcept {
    return minLevel_.load(std::memory_order_relaxed);
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level >= minLevel()) {
        std::string formatted = formatPrefix(level) + message;
        write(formatted);
    }
}

void Logger::trace(const std::string& message) {
    log(LogLevel::Trace, message);
}

void Logger::debug(const std::string& message) {
    log(LogLevel::Debug, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::Info, message);
}

void Logger::warn(const std::string& message) {
    log(LogLevel::Warn, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::Error, message);
}

Logger& Logger::instance() {
    static Logger globalLogger;
    return globalLogger;
}

std::string Logger::formatPrefix(LogLevel level) const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << "[" 
        << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count()
        << "] [" << logLevelToString(level) << "] ";
    
    return oss.str();
}

void Logger::write(const std::string& formattedMessage) {
    std::lock_guard<std::mutex> lock(mutex_);
    *output_ << formattedMessage << std::endl;
    output_->flush();
}

}  // namespace demo_daemon
