#pragma once

#include "demo_daemon/ipc/connection_manager.hpp"
#include "demo_daemon/ipc/json_protocol.hpp"
#include "demo_daemon/core/logger.hpp"

#include <sys/epoll.h>
#include <sys/un.h>
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <functional>
#include <optional>
#include <chrono>

namespace demo_daemon::ipc {

/**
 * @brief Конфигурация Unix Domain Socket сервера
 */
struct UnixSocketConfig {
    std::string socket_path{"/run/demo_daemon/demo_daemon.sock"};
    int socket_permissions{0660};
    size_t max_connections{100};
    std::chrono::milliseconds read_timeout{30000};
    std::chrono::milliseconds write_timeout{30000};
    size_t max_message_size{1024 * 1024}; // 1 MiB
    int backlog{128};
};

/**
 * @brief Unix Domain Socket сервер на базе epoll
 * 
 * Обрабатывает множественные клиентские соединения используя
 * edge-triggered epoll для высокой производительности.
 */
class UnixSocketServer {
public:
    using CommandHandler = std::function<ResponseMessage(const RequestMessage&)>;
    
    explicit UnixSocketServer(UnixSocketConfig config,
                              std::shared_ptr<Logger> logger);
    
    ~UnixSocketServer();
    
    // Запрещаем копирование и перемещение
    UnixSocketServer(const UnixSocketServer&) = delete;
    UnixSocketServer& operator=(const UnixSocketServer&) = delete;
    UnixSocketServer(UnixSocketServer&&) = delete;
    UnixSocketServer& operator=(UnixSocketServer&&) = delete;
    
    /**
     * @brief Инициализировать сервер (создать сокет, bind, listen)
     * @return true если успешно
     */
    [[nodiscard]] bool initialize();
    
    /**
     * @brief Запустить сервер в текущем потоке
     * @param command_handler обработчик команд
     * @return true если сервер остановлен корректно
     */
    [[nodiscard]] bool run(CommandHandler command_handler);
    
    /**
     * @brief Остановить сервер
     */
    void stop();
    
    /**
     * @brief Проверить, запущен ли сервер
     */
    [[nodiscard]] bool is_running() const noexcept { return running_; }
    
    /**
     * @brief Получить путь к сокету
     */
    [[nodiscard]] const std::string& get_socket_path() const noexcept { 
        return config_.socket_path; 
    }
    
    /**
     * @brief Получить количество активных подключений
     */
    [[nodiscard]] size_t active_connections() const;
    
    /**
     * @brief Удалить файл сокета (если существует)
     * @param socket_path путь к сокету
     * @return true если файл удален или не существовал
     */
    [[nodiscard]] static bool cleanup_socket(const std::string& socket_path);
    
private:
    /**
     * @brief Создать сокет сервера
     * @return файловый дескриптор или -1 при ошибке
     */
    [[nodiscard]] int create_server_socket();
    
    /**
     * @brief Добавить FD в epoll
     * @param fd файловый дескриптор
     * @param events события для отслеживания
     * @param data данные для event
     * @return true если успешно
     */
    [[nodiscard]] bool add_to_epoll(int fd, uint32_t events, void* data);
    
    /**
     * @brief Обновить события в epoll
     * @param fd файловый дескриптор
     * @param events новые события
     * @param data данные для event
     * @return true если успешно
     */
    [[nodiscard]] bool modify_epoll(int fd, uint32_t events, void* data);
    
    /**
     * @brief Удалить FD из epoll
     * @param fd файловый дескриптор
     * @return true если успешно
     */
    [[nodiscard]] bool remove_from_epoll(int fd);
    
    /**
     * @brief Принять новое соединение
     * @return файловый дескриптор клиента или -1
     */
    [[nodiscard]] int accept_connection();
    
    /**
     * @brief Обработать событие epoll
     * @param event событие
     * @param command_handler обработчик команд
     */
    void handle_epoll_event(const struct epoll_event& event,
                           CommandHandler command_handler);
    
    /**
     * @brief Обработчик сообщений от сессий
     * @param msg запрос
     * @param session сессия
     * @param command_handler обработчик команд
     */
    void on_message(const RequestMessage& msg,
                   std::shared_ptr<Session> session,
                   CommandHandler command_handler);
    
    UnixSocketConfig config_;
    std::shared_ptr<Logger> logger_;
    
    int server_fd_{-1};
    int epoll_fd_{-1};
    
    ConnectionManager connection_manager_;
    
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
};

} // namespace demo_daemon::ipc
