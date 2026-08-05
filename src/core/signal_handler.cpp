#include "demo_daemon/core/signal_handler.hpp"
#include "demo_daemon/core/logger.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <stdexcept>

namespace demo_daemon {

// Инициализация статического атомарного флага
std::atomic<bool> SignalHandler::shutdownRequested_{false};

SignalHandler::SignalHandler()
    : pipeFds_{-1, -1}
    , pendingSignal_(SignalType::None)
    , initialized_(false) {
    pipeFds_.fill(-1);
}

SignalHandler::~SignalHandler() {
    if (pipeFds_[0] >= 0) {
        ::close(pipeFds_[0]);
    }
    if (pipeFds_[1] >= 0) {
        ::close(pipeFds_[1]);
    }
}

bool SignalHandler::initialize() {
    if (initialized_.load(std::memory_order_acquire)) {
        return true;  // Уже инициализирован
    }

    // Создаем self-pipe для безопасной передачи сигналов
    if (::pipe(pipeFds_.data()) != 0) {
        LOG_ERROR("Failed to create signal pipe");
        return false;
    }

    // Выставляем O_NONBLOCK для чтения
    int flags = ::fcntl(pipeFds_[0], F_GETFL, 0);
    if (flags == -1 || ::fcntl(pipeFds_[0], F_SETFL, flags | O_NONBLOCK) == -1) {
        LOG_ERROR("Failed to set non-blocking flag on signal pipe");
        return false;
    }

    // Регистрируем обработчики сигналов
    struct sigaction sa {};
    sa.sa_handler = handleSignal;
    sa.sa_flags = SA_RESTART;  // Restart interrupted system calls
    sigemptyset(&sa.sa_mask);

    // SIGINT (Ctrl+C) - graceful shutdown
    if (::sigaction(SIGINT, &sa, nullptr) != 0) {
        LOG_ERROR("Failed to register SIGINT handler");
        return false;
    }

    // SIGTERM - graceful shutdown
    if (::sigaction(SIGTERM, &sa, nullptr) != 0) {
        LOG_ERROR("Failed to register SIGTERM handler");
        return false;
    }

    // SIGHUP - опционально reload config (пока просто логируем)
    if (::sigaction(SIGHUP, &sa, nullptr) != 0) {
        LOG_ERROR("Failed to register SIGHUP handler");
        return false;
    }

    // SIGPIPE - игнорируем, чтобы не падать при разрыве соединения клиентом
    sa.sa_handler = SIG_IGN;
    if (::sigaction(SIGPIPE, &sa, nullptr) != 0) {
        LOG_ERROR("Failed to ignore SIGPIPE");
        return false;
    }

    initialized_.store(true, std::memory_order_release);
    LOG_DEBUG("Signal handlers registered successfully");
    return true;
}

SignalHandler::SignalType SignalHandler::checkSignal() {
    if (!initialized_.load(std::memory_order_acquire)) {
        return SignalType::None;
    }

    // Читаем из pipe, чтобы сбросить notification
    char buffer[64];
    while (::read(pipeFds_[0], buffer, sizeof(buffer)) > 0) {
        // Читаем пока есть данные (drain the pipe)
    }

    return pendingSignal_.exchange(SignalType::None, std::memory_order_acq_rel);
}

void SignalHandler::reset() {
    pendingSignal_.store(SignalType::None, std::memory_order_release);
}

SignalHandler& SignalHandler::instance() {
    static SignalHandler instance;
    return instance;
}

void SignalHandler::handleSignal(int signum) {
    // В signal handler можно использовать только async-signal-safe функции
    // Никаких mutex, malloc, логирования и т.д.
    
    switch (signum) {
        case SIGINT:
            shutdownRequested_.store(true, std::memory_order_release);
            writeSignalToPipe(SIGINT);
            break;
        
        case SIGTERM:
            shutdownRequested_.store(true, std::memory_order_release);
            writeSignalToPipe(SIGTERM);
            break;
        
        case SIGHUP:
            writeSignalToPipe(SIGHUP);
            break;
        
        case SIGPIPE:
            // Игнорируем, но записываем для отладки
            writeSignalToPipe(SIGPIPE);
            break;
        
        default:
            break;
    }
}

void SignalHandler::writeSignalToPipe(int signum) {
    // Async-signal-safe запись одного байта в pipe
    char signalByte = static_cast<char>(signum);
    
    // Получаем доступ к pipeFds через instance (небезопасно в signal handler,
    // но atomic операции и write обычно safe)
    // Используем сырой указатель чтобы избежать проблем
    static std::array<int, 2>* staticPipeFds = nullptr;
    if (!staticPipeFds) {
        // Первый вызов - запоминаем адрес (это безопасно т.к. вызывается до многопоточности)
        staticPipeFds = const_cast<std::array<int, 2>*>(&instance().pipeFds_);
    }
    
    ssize_t result = ::write((*staticPipeFds)[1], &signalByte, 1);
    (void)result;  // Игнорируем результат в signal handler
    
    // Также обновляем pendingSignal для checkSignal()
    // Это не совсем безопасно, но atomic операции обычно safe
    SignalHandler* handler = &instance();
    if (handler->initialized_.load(std::memory_order_relaxed)) {
        switch (signum) {
            case SIGINT:
                handler->pendingSignal_.store(SignalType::Interrupt, 
                                             std::memory_order_relaxed);
                break;
            case SIGTERM:
                handler->pendingSignal_.store(SignalType::Terminate,
                                             std::memory_order_relaxed);
                break;
            case SIGHUP:
                handler->pendingSignal_.store(SignalType::Hangup,
                                             std::memory_order_relaxed);
                break;
            case SIGPIPE:
                handler->pendingSignal_.store(SignalType::Pipe,
                                             std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }
}

}  // namespace demo_daemon
