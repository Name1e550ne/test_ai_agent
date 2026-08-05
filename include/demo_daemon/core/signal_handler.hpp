#pragma once

#include <csignal>
#include <atomic>
#include <functional>
#include <mutex>
#include <array>
#include "demo_daemon/core/types.hpp"

namespace demo_daemon {

/// Безопасный обработчик сигналов для graceful shutdown
/// 
/// Signal handler должен быть минимальным и async-signal-safe.
/// Этот класс использует atomic flag и self-pipe trick для безопасной
/// передачи сигналов в основной цикл обработки.
class SignalHandler {
public:
    // Типы сигналов, которые мы обрабатываем
    enum class SignalType {
        None = 0,
        Interrupt,    // SIGINT
        Terminate,    // SIGTERM
        Hangup,       // SIGHUP (optional reload)
        Pipe          // SIGPIPE (ignored)
    };

    SignalHandler();
    ~SignalHandler();

    // Запрет копирования и перемещения
    SignalHandler(const SignalHandler&) = delete;
    SignalHandler& operator=(const SignalHandler&) = delete;
    SignalHandler(SignalHandler&&) = delete;
    SignalHandler& operator=(SignalHandler&&) = delete;

    /// Инициализация обработчиков сигналов
    [[nodiscard]] bool initialize();

    /// Проверка наличия ожидающего сигнала
    [[nodiscard]] SignalType checkSignal();

    /// Сброс состояния сигнала после обработки
    void reset();

    /// Получить singleton
    static SignalHandler& instance();

    /// Проверка флага shutdown из любого места
    [[nodiscard]] static bool isShutdownRequested() noexcept;

private:
    /// Статический signal handler (должен быть C-style функцией)
    static void handleSignal(int signum);

    /// Async-signal-safe запись в pipe
    static void writeSignalToPipe(int signum);

    std::array<int, 2> pipeFds_;  // self-pipe for signal notification
    std::atomic<SignalType> pendingSignal_;
    std::atomic<bool> initialized_;
    
    // Глобальный флаг shutdown для быстрой проверки
    static std::atomic<bool> shutdownRequested_;
};

[[nodiscard]] inline bool SignalHandler::isShutdownRequested() noexcept {
    return shutdownRequested_.load(std::memory_order_acquire);
}

}  // namespace demo_daemon
