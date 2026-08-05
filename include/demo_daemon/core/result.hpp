#pragma once

#include <string>
#include <variant>
#include <type_traits>

namespace demo_daemon {

/// Коды ошибок для Result
enum class ErrorCode {
    Success = 0,
    InvalidArgument = 1,
    NotFound = 2,
    AlreadyExists = 3,
    PermissionDenied = 4,
    Timeout = 5,
    InternalError = 6,
    NotInitialized = 7,
    Cancelled = 8
};

[[nodiscard]] inline std::string errorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::Success:          return "Success";
        case ErrorCode::InvalidArgument:  return "InvalidArgument";
        case ErrorCode::NotFound:         return "NotFound";
        case ErrorCode::AlreadyExists:    return "AlreadyExists";
        case ErrorCode::PermissionDenied: return "PermissionDenied";
        case ErrorCode::Timeout:          return "Timeout";
        case ErrorCode::InternalError:    return "InternalError";
        case ErrorCode::NotInitialized:   return "NotInitialized";
        case ErrorCode::Cancelled:        return "Cancelled";
        default:                          return "UnknownError";
    }
}

/// Error объект с кодом и сообщением
struct Error {
    ErrorCode code;
    std::string message;

    Error() : code(ErrorCode::Success), message() {}
    
    explicit Error(ErrorCode c, std::string msg = "")
        : code(c), message(std::move(msg)) {}

    [[nodiscard]] bool ok() const noexcept {
        return code == ErrorCode::Success;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return !ok();
    }
};

/// Result тип для обработки ошибок без исключений
/// Используется для ожидаемых ошибок (не exceptional cases)
template<typename T>
class [[nodiscard]] Result {
public:
    using value_type = T;
    using error_type = Error;

    // Конструктор успешного результата
    explicit Result(T value) 
        : data_(std::move(value)) {}

    // Конструктор ошибки
    explicit Result(Error error) 
        : data_(std::move(error)) {}

    // Factory методы
    [[nodiscard]] static Result ok(T value) {
        return Result(std::move(value));
    }

    [[nodiscard]] static Result err(ErrorCode code, std::string message = "") {
        return Result(Error(code, std::move(message)));
    }

    [[nodiscard]] static Result err(Error error) {
        return Result(std::move(error));
    }

    // Проверка состояния
    [[nodiscard]] bool ok() const noexcept {
        return std::holds_alternative<T>(data_);
    }

    [[nodiscard]] bool hasError() const noexcept {
        return !ok();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return ok();
    }

    // Получение значения
    [[nodiscard]] T& value() & {
        if (!ok()) {
            throw std::runtime_error("Result does not contain value: " + 
                                    error().message);
        }
        return std::get<T>(data_);
    }

    [[nodiscard]] const T& value() const& {
        if (!ok()) {
            throw std::runtime_error("Result does not contain value: " + 
                                    error().message);
        }
        return std::get<T>(data_);
    }

    [[nodiscard]] T&& value() && {
        if (!ok()) {
            throw std::runtime_error("Result does not contain value: " + 
                                    error().message);
        }
        return std::get<T>(std::move(data_));
    }

    [[nodiscard]] const T&& value() const&& {
        if (!ok()) {
            throw std::runtime_error("Result does not contain value: " + 
                                    error().message);
        }
        return std::get<T>(std::move(data_));
    }

    // Получение ошибки
    [[nodiscard]] Error& error() & {
        if (ok()) {
            throw std::runtime_error("Result does not contain error");
        }
        return std::get<Error>(data_);
    }

    [[nodiscard]] const Error& error() const& {
        if (ok()) {
            throw std::runtime_error("Result does not contain error");
        }
        return std::get<Error>(data_);
    }

    // Получение значения или дефолта
    [[nodiscard]] T valueOr(T defaultValue) const& {
        if (ok()) {
            return std::get<T>(data_);
        }
        return std::move(defaultValue);
    }

    // Получение значения или вычисление дефолта
    template<typename F>
    [[nodiscard]] T valueOrInvoke(F&& func) const& {
        if (ok()) {
            return std::get<T>(data_);
        }
        return std::forward<F>(func)(error());
    }

private:
    std::variant<T, Error> data_;
};

// Специализация для void
template<>
class [[nodiscard]] Result<void> {
public:
    using error_type = Error;

    Result() = default;

    explicit Result(Error error) 
        : error_(std::move(error)), has_value_(true) {}

    [[nodiscard]] static Result ok() {
        return Result();
    }

    [[nodiscard]] static Result err(ErrorCode code, std::string message = "") {
        return Result(Error(code, std::move(message)));
    }

    [[nodiscard]] static Result err(Error error) {
        return Result(std::move(error));
    }

    [[nodiscard]] bool isSuccess() const noexcept {
        return has_value_;
    }

    [[nodiscard]] bool hasError() const noexcept {
        return !has_value_;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return has_value_;
    }

    [[nodiscard]] Error& error() & {
        if (has_value_) {
            throw std::runtime_error("Result does not contain error");
        }
        return error_;
    }

    [[nodiscard]] const Error& error() const& {
        if (has_value_) {
            throw std::runtime_error("Result does not contain error");
        }
        return error_;
    }

private:
    Error error_;
    bool has_value_ = true;
};

}  // namespace demo_daemon
