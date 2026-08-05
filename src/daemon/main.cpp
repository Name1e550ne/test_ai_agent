#include "demo_daemon/core/signal_handler.hpp"
#include "demo_daemon/core/logger.hpp"
#include "demo_daemon/core/version.hpp"
#include "demo_daemon/ipc/unix_socket_server.hpp"
#include "demo_daemon/tasks/task_manager.hpp"
#include "demo_daemon/tasks/task_registry.hpp"
#include "demo_daemon/core/command_registry.hpp"
#include <iostream>
#include <csignal>
#include <unistd.h>
#include <cstdlib>

using namespace demo_daemon;

namespace {

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n"
              << "Options:\n"
              << "  --socket-path PATH   Unix socket path (default: /tmp/demo_daemon.sock)\n"
              << "  --log-level LEVEL    Log level: trace, debug, info, warn, error (default: info)\n"
              << "  --help               Show this help message\n"
              << "  --version            Show version information\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    // Парсим аргументы командной строки
    std::string socketPath = "/tmp/demo_daemon.sock";
    LogLevel logLevel = LogLevel::Info;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            printUsage(argv[0]);
            return EXIT_SUCCESS;
        }
        
        if (arg == "--version") {
            std::cout << "Demo Daemon v" << VERSION_MAJOR << "." 
                      << VERSION_MINOR << "." << VERSION_PATCH << "\n";
            return EXIT_SUCCESS;
        }
        
        if (arg == "--socket-path" && i + 1 < argc) {
            socketPath = argv[++i];
        } else if (arg == "--log-level" && i + 1 < argc) {
            std::string level = argv[++i];
            if (level == "trace") logLevel = LogLevel::Trace;
            else if (level == "debug") logLevel = LogLevel::Debug;
            else if (level == "info") logLevel = LogLevel::Info;
            else if (level == "warn") logLevel = LogLevel::Warn;
            else if (level == "error") logLevel = LogLevel::Error;
            else {
                std::cerr << "Unknown log level: " << level << "\n";
                return EXIT_FAILURE;
            }
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    // Устанавливаем уровень логирования
    Logger::instance().setMinLevel(logLevel);

    LOG_INFO("Starting Demo Daemon v" + std::to_string(VERSION_MAJOR) + "." + 
             std::to_string(VERSION_MINOR) + "." + std::to_string(VERSION_PATCH));
    LOG_INFO("Socket path: " + socketPath);

    // Инициализируем обработчик сигналов
    SignalHandler& signalHandler = SignalHandler::instance();
    if (!signalHandler.initialize()) {
        LOG_ERROR("Failed to initialize signal handler");
        return EXIT_FAILURE;
    }

    // Главный цикл обработки с поддержкой graceful shutdown
    while (!SignalHandler::isShutdownRequested()) {
        // Проверяем наличие сигналов
        SignalHandler::SignalType signal = signalHandler.checkSignal();
        
        if (signal != SignalHandler::SignalType::None) {
            switch (signal) {
                case SignalHandler::SignalType::Interrupt:
                    LOG_INFO("Received SIGINT, initiating graceful shutdown...");
                    break;
                    
                case SignalHandler::SignalType::Terminate:
                    LOG_INFO("Received SIGTERM, initiating graceful shutdown...");
                    break;
                    
                case SignalHandler::SignalType::Hangup:
                    LOG_INFO("Received SIGHUP, config reload not implemented yet");
                    signalHandler.reset();
                    continue;
                    
                case SignalHandler::SignalType::Pipe:
                    // Игнорируем SIGPIPE
                    signalHandler.reset();
                    continue;
                    
                default:
                    break;
            }
            break;  // Выход из цикла при SIGINT/SIGTERM
        }

        // Небольшая пауза чтобы не нагружать CPU и проверять сигналы
        usleep(100000);  // 100ms
    }

    // Graceful shutdown
    LOG_INFO("Shutting down daemon...");
    LOG_INFO("Daemon stopped gracefully");
    
    return EXIT_SUCCESS;
}
