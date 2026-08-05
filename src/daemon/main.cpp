#include "demo_daemon/core/signal_handler.hpp"
#include "demo_daemon/core/logger.hpp"
#include "demo_daemon/core/version.hpp"
#include "demo_daemon/core/command_registry.hpp"
#include "demo_daemon/ipc/unix_socket_server.hpp"
#include "demo_daemon/tasks/task_manager.hpp"
#include "demo_daemon/tasks/task_registry.hpp"
#include "demo_daemon/commands/ping_command.hpp"
#include "demo_daemon/commands/status_command.hpp"
#include "demo_daemon/commands/shutdown_command.hpp"
#include "demo_daemon/commands/commands_list_command.hpp"
#include "demo_daemon/commands/tasks_list_command.hpp"
#include "demo_daemon/commands/tasks_add_command.hpp"
#include "demo_daemon/commands/tasks_stop_command.hpp"
#include <iostream>
#include <csignal>
#include <unistd.h>
#include <cstdlib>
#include <memory>
#include <thread>
#include <chrono>

using namespace demo_daemon;
using namespace demo_daemon::ipc;
using namespace demo_daemon::tasks;
using namespace demo_daemon::core;

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

    // Создаем реестр команд и регистрируем встроенные команды
    auto commandRegistry = std::make_shared<CommandRegistry>();
    
    // Регистрируем встроенные команды
    commandRegistry->register_command(std::make_shared<PingCommand>());
    commandRegistry->register_command(std::make_shared<StatusCommand>());
    commandRegistry->register_command(std::make_shared<ShutdownCommand>(signalHandler));
    commandRegistry->register_command(std::make_shared<CommandsListCommand>(*commandRegistry));
    
    // Создаем реестр задач и менеджер задач
    auto taskRegistry = std::make_shared<TaskRegistry>();
    tasks::TaskRegistry::register_builtin_tasks(taskRegistry);
    auto taskManager = std::make_shared<TaskManager>(taskRegistry);
    
    // Добавляем команды для управления задачами
    commandRegistry->register_command(std::make_shared<TasksListCommand>(*taskManager));
    commandRegistry->register_command(std::make_shared<TasksAddCommand>(*taskManager));
    commandRegistry->register_command(std::make_shared<TasksStopCommand>(*taskManager));

    // Настраиваем сервер
    UnixSocketConfig config;
    config.socket_path = socketPath;
    auto logger = std::make_shared<Logger>();
    auto server = std::make_unique<UnixSocketServer>(config, logger);
    
    if (!server->initialize()) {
        LOG_ERROR("Failed to initialize Unix socket server");
        return EXIT_FAILURE;
    }

    LOG_INFO("Unix socket server initialized at " + socketPath);
    LOG_INFO("Daemon is ready to accept connections");

    // Обработчик команд для сервера
    auto commandHandler = [commandRegistry, taskManager](const RequestMessage& request) -> ResponseMessage {
        try {
            auto cmdOpt = commandRegistry->get_command(request.method);
            if (!cmdOpt) {
                nlohmann::json error_data = {
                    {"code", -32601},
                    {"message", "Method not found: " + request.method}
                };
                return ResponseMessage{
                    .id = request.id,
                    .result = error_data
                };
            }
            
            auto cmd = *cmdOpt;
            core::CommandContext ctx{};
            ctx.task_manager = taskManager;
            auto result = cmd->execute(ctx, request.params);
            
            nlohmann::json response_data = result.data;
            if (!result.success) {
                nlohmann::json error_data = {
                    {"code", result.error_code},
                    {"message", result.message}
                };
                return ResponseMessage{
                    .id = request.id,
                    .result = error_data
                };
            }
            
            return ResponseMessage{
                .id = request.id,
                .result = std::move(response_data)
            };
        } catch (const std::exception& e) {
            LOG_ERROR("Command execution error: " + std::string(e.what()));
            nlohmann::json error_data = {
                {"code", -32603},
                {"message", "Internal error: " + std::string(e.what())}
            };
            return ResponseMessage{
                .id = request.id,
                .result = error_data
            };
        }
    };

    // Запускаем сервер в отдельном потоке
    std::thread serverThread([&server, commandHandler]() {
        server->run(commandHandler);
    });

    // Главный цикл обработки сигналов
    while (!SignalHandler::isShutdownRequested()) {
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

        // Небольшая пауза чтобы не нагружать CPU
        usleep(100000);  // 100ms
    }

    // Graceful shutdown
    LOG_INFO("Shutting down daemon...");
    
    // Останавливаем сервер
    LOG_INFO("Stopping Unix socket server...");
    server->stop();
    
    // Ждем завершения потока сервера
    if (serverThread.joinable()) {
        serverThread.join();
    }
    
    // Останавливаем все задачи
    LOG_INFO("Stopping all background tasks...");
    taskManager->stop_all_tasks();
    
    LOG_INFO("Daemon stopped gracefully");
    
    return EXIT_SUCCESS;
}
