#include "demo_daemon/cli/client.hpp"
#include "demo_daemon/core/logger.hpp"
#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <getopt.h>
#include <nlohmann/json.hpp>

namespace {

struct CliOptions {
    std::string socket_path;
    int timeout_ms = 5000;
    bool raw_json = false;
    bool help = false;
    std::string command;
    std::vector<std::string> args;
    nlohmann::json params;
};

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [OPTIONS] COMMAND [ARGS...]\n"
              << "\nCLI client for demo_daemon\n"
              << "\nOptions:\n"
              << "  --socket-path PATH   Path to Unix Domain Socket (default: /run/demo_daemon/demo_daemon.sock)\n"
              << "  --timeout MS         Timeout in milliseconds (default: 5000)\n"
              << "  --raw-json           Output raw JSON response\n"
              << "  --help               Show this help message\n"
              << "\nCommands:\n"
              << "  ping                 Check daemon connectivity\n"
              << "  status               Get daemon status\n"
              << "  shutdown             Request daemon shutdown\n"
              << "  commands list        List available commands\n"
              << "  tasks list           List background tasks\n"
              << "  tasks add TYPE       Add a background task (TYPE: heartbeat, sleep)\n"
              << "  tasks stop ID        Stop a background task by ID\n"
              << "  call METHOD PARAMS   Call arbitrary method with JSON params\n"
              << "\nExamples:\n"
              << "  " << program << " ping\n"
              << "  " << program << " status\n"
              << "  " << program << " tasks add heartbeat --interval 5\n"
              << "  " << program << " call my_method '{\"key\":\"value\"}'\n";
}

std::string getDefaultSocketPath() {
    const char* env_path = std::getenv("DEMO_DAEMON_SOCKET");
    if (env_path && env_path[0] != '\0') {
        return env_path;
    }
    
    const char* xdg_runtime = std::getenv("XDG_RUNTIME_DIR");
    if (xdg_runtime && xdg_runtime[0] != '\0') {
        return std::string(xdg_runtime) + "/demo_daemon.sock";
    }
    
    return "/run/demo_daemon/demo_daemon.sock";
}

CliOptions parseOptions(int argc, char* argv[]) {
    CliOptions opts;
    opts.socket_path = getDefaultSocketPath();
    
    static struct option long_options[] = {
        {"socket-path", required_argument, nullptr, 's'},
        {"timeout",     required_argument, nullptr, 't'},
        {"raw-json",    no_argument,       nullptr, 'r'},
        {"help",        no_argument,       nullptr, 'h'},
        {nullptr,       0,                 nullptr, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "s:t:rh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 's':
                opts.socket_path = optarg;
                break;
            case 't':
                opts.timeout_ms = std::atoi(optarg);
                break;
            case 'r':
                opts.raw_json = true;
                break;
            case 'h':
                opts.help = true;
                break;
            default:
                break;
        }
    }
    
    // Оставшиеся аргументы - команда и её параметры
    if (optind < argc) {
        opts.command = argv[optind++];
    }
    
    while (optind < argc) {
        opts.args.push_back(argv[optind++]);
    }
    
    return opts;
}

nlohmann::json parseTaskArgs(const std::vector<std::string>& args) {
    nlohmann::json params = nlohmann::json::object();
    
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--interval" && i + 1 < args.size()) {
            params["interval"] = std::stoi(args[++i]);
        } else if (args[i] == "--duration" && i + 1 < args.size()) {
            params["duration"] = std::stoi(args[++i]);
        } else if (args[i] == "--type") {
            params["type"] = args[++i];
        } else if (args[i].find('=') != std::string::npos) {
            auto pos = args[i].find('=');
            std::string key = args[i].substr(0, pos);
            std::string value = args[i].substr(pos + 1);
            // Пытаемся распарсить как число
            try {
                params[key] = std::stoi(value);
            } catch (...) {
                params[key] = value;
            }
        }
    }
    
    return params;
}

int formatAndPrintResponse(const demo_daemon::cli::CommandResponse& response, 
                           const std::string& command,
                           const std::vector<std::string>& command_args,
                           bool raw_json) {
    if (raw_json) {
        nlohmann::json output;
        output["success"] = response.success;
        if (response.error_code) {
            output["error_code"] = *response.error_code;
        }
        output["message"] = response.message;
        output["data"] = response.data;
        output["response_time_ms"] = response.response_time.count();
        std::cout << output.dump(2) << std::endl;
    } else {
        // Human-readable вывод
        if (!response.success) {
            std::cerr << "Error: " << response.message;
            if (response.error_code) {
                std::cerr << " (code: " << *response.error_code << ")";
            }
            std::cerr << std::endl;
            return 1;
        }
        
        // Форматируем вывод в зависимости от команды
        if (command == "ping") {
            std::cout << "Pong! Response time: " << response.response_time.count() << "ms" << std::endl;
        } else if (command == "status") {
            std::cout << "Daemon Status:" << std::endl;
            if (response.data.contains("version")) {
                std::cout << "  Version: " << response.data["version"] << std::endl;
            }
            if (response.data.contains("uptime_seconds")) {
                std::cout << "  Uptime: " << response.data["uptime_seconds"] << "s" << std::endl;
            }
            if (response.data.contains("active_clients")) {
                std::cout << "  Active clients: " << response.data["active_clients"] << std::endl;
            }
            if (response.data.contains("tasks")) {
                std::cout << "  Tasks: " << response.data["tasks"].size() << std::endl;
            }
        } else if (command == "commands" && !command_args.empty() && command_args[0] == "list") {
            std::cout << "Available commands:" << std::endl;
            if (response.data.is_array()) {
                for (const auto& cmd : response.data) {
                    std::string name = cmd.value("name", "unknown");
                    std::string desc = cmd.value("description", "");
                    std::cout << "  " << name;
                    if (!desc.empty()) {
                        std::cout << " - " << desc;
                    }
                    std::cout << std::endl;
                }
            }
        } else if (command == "tasks" && !command_args.empty()) {
            if (command_args[0] == "list") {
                std::cout << "Background tasks:" << std::endl;
                if (response.data.is_array()) {
                    for (const auto& task : response.data) {
                        std::cout << "  ID: " << task.value("id", -1)
                                  << ", Type: " << task.value("type", "unknown")
                                  << ", State: " << task.value("state", "unknown")
                                  << std::endl;
                    }
                }
            } else if (command_args[0] == "add") {
                std::cout << "Task added. ID: " << response.data.value("task_id", -1) << std::endl;
            } else if (command_args[0] == "stop") {
                std::cout << "Task stopped." << std::endl;
            }
        } else if (command == "shutdown") {
            std::cout << "Shutdown request sent." << std::endl;
        } else if (command == "call") {
            std::cout << response.data.dump(2) << std::endl;
        } else {
            // Общий вывод
            std::cout << response.data.dump(2) << std::endl;
        }
    }
    
    return response.success ? 0 : 1;
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    auto opts = parseOptions(argc, argv);
    
    if (opts.help || opts.command.empty()) {
        printUsage(argv[0]);
        return opts.help ? 0 : 1;
    }
    
    // Создаем клиента
    demo_daemon::cli::DemoDaemonClient client(
        opts.socket_path, 
        std::chrono::milliseconds(opts.timeout_ms)
    );
    
    // Подключаемся
    if (!client.connect()) {
        std::cerr << "Failed to connect to daemon at: " << opts.socket_path << std::endl;
        std::cerr << "Make sure the daemon is running and socket path is correct." << std::endl;
        return 1;
    }
    
    // Формируем запрос в зависимости от команды
    std::string method;
    nlohmann::json params = opts.params;
    
    if (opts.command == "ping") {
        method = "ping";
    } else if (opts.command == "status") {
        method = "status";
    } else if (opts.command == "shutdown") {
        method = "shutdown";
    } else if (opts.command == "commands" && !opts.args.empty() && opts.args[0] == "list") {
        method = "commands.list";
    } else if (opts.command == "tasks") {
        if (!opts.args.empty()) {
            if (opts.args[0] == "list") {
                method = "tasks.list";
            } else if (opts.args[0] == "add" && opts.args.size() > 1) {
                method = "tasks.add";
                params = parseTaskArgs(std::vector<std::string>(opts.args.begin() + 1, opts.args.end()));
                if (!params.contains("type")) {
                    params["type"] = opts.args[1];
                }
            } else if (opts.args[0] == "stop" && opts.args.size() > 1) {
                method = "tasks.stop";
                params["task_id"] = std::stoi(opts.args[1]);
            } else {
                std::cerr << "Unknown tasks subcommand: " << opts.args[0] << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Tasks subcommand required (list, add, stop)" << std::endl;
            return 1;
        }
    } else if (opts.command == "call" && opts.args.size() >= 2) {
        method = opts.args[0];
        try {
            params = nlohmann::json::parse(opts.args[1]);
        } catch (const std::exception& e) {
            std::cerr << "Invalid JSON params: " << e.what() << std::endl;
            return 1;
        }
    } else {
        std::cerr << "Unknown command: " << opts.command << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Отправляем запрос
    auto response = client.sendRequest(method, params);
    
    // Выводим результат
    return formatAndPrintResponse(response, opts.command, opts.args, opts.raw_json);
}
