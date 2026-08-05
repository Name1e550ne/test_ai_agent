#pragma once

#include <cstdint>
#include <string>
#include <chrono>

namespace demo_daemon {

// Версия сервиса
struct Version {
    int major = 0;
    int minor = 1;
    int patch = 0;
    std::string suffix;  // e.g., "dev", "rc1"

    [[nodiscard]] std::string toString() const {
        std::string result = std::to_string(major) + "." + 
                            std::to_string(minor) + "." + 
                            std::to_string(patch);
        if (!suffix.empty()) {
            result += "-" + suffix;
        }
        return result;
    }
};

[[nodiscard]] inline Version getCurrentVersion() {
    return Version{0, 1, 0, "dev"};
}

// Уровни логирования
enum class LogLevel : int {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4
};

[[nodiscard]] inline std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        default: return "UNKNOWN";
    }
}

[[nodiscard]] inline LogLevel stringToLogLevel(const std::string& str) {
    if (str == "trace" || str == "TRACE") return LogLevel::Trace;
    if (str == "debug" || str == "DEBUG") return LogLevel::Debug;
    if (str == "info" || str == "INFO")   return LogLevel::Info;
    if (str == "warn" || str == "WARN")   return LogLevel::Warn;
    if (str == "error" || str == "ERROR") return LogLevel::Error;
    return LogLevel::Info;  // default
}

// Типы для времени
using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration = Clock::duration;

// Размер буфера по умолчанию
inline constexpr std::size_t kDefaultBufferSize = 65536;  // 64 KB
inline constexpr std::size_t kMaxMessageSize = 1024 * 1024;  // 1 MB

}  // namespace demo_daemon
