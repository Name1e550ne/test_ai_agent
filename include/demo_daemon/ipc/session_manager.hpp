#pragma once

#include "demo_daemon/ipc/session.hpp"
#include "demo_daemon/ipc/json_protocol.hpp"
#include "demo_daemon/core/logger.hpp"
#include "demo_daemon/core/result.hpp"

#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <memory>
#include <functional>
#include <optional>

namespace demo_daemon::ipc {

class ConnectionManager;

/**
 * @brief Сессия клиента - обрабатывает одно соединение
 * 
 * Управляет чтением/записью данных, парсингом JSON-сообщений
 * и передачей обработанных команд в обработчик.
 */
class Session : public std::enable_shared_from_this<Session> {
public:
    using MessageHandler = std::function<void(const RequestMessage&)>;
    using ErrorHandler = std::function<void(const ProtocolError&)>;
    
    explicit Session(int client_fd, 
                     std::shared_ptr<Logger> logger,
                     MessageHandler message_handler,
                     ErrorHandler error_handler);
    
    ~Session();
    
    // Запрещаем копирование
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    
    // Разрешаем перемещение
    Session(Session&&) = default;
    Session& operator=(Session&&) = delete;
    
    [[nodiscard]] int get_fd() const noexcept { return client_fd_; }
    [[nodiscard]] SessionState get_state() const noexcept { return state_; }
    [[nodiscard]] uint64_t get_id() const noexcept { return session_id_; }
    
    /**
     * @brief Подготовить сессию к работе (настроить non-blocking mode)
     * @return true если успешно
     */
    [[nodiscard]] bool initialize();
    
    /**
     * @brief Обработать событие чтения
     * @return true если сессия активна, false если нужно закрыть
     */
    [[nodiscard]] bool on_readable();
    
    /**
     * @brief Обработать событие записи
     * @return true если сессия активна, false если нужно закрыть
     */
    [[nodiscard]] bool on_writable();
    
    /**
     * @brief Запланировать отправку сообщения
     * @param response сообщение для отправки
     */
    void schedule_send(const ResponseMessage& response);
    
    /**
     * @brief Запланировать отправку ошибки
     * @param error ошибка
     */
    void schedule_send_error(const ErrorResponseMessage& error_response);
    
    /**
     * @brief Начать закрытие сессии
     */
    void close();
    
    /**
     * @brief Проверить, закрыта ли сессия
     */
    [[nodiscard]] bool is_closed() const noexcept { 
        return state_ == SessionState::Closed; 
    }
    
private:
    /**
     * @brief Прочитать данные из сокета
     * @return количество прочитанных байт, 0 если EOF, -1 если ошибка
     */
    [[nodiscard]] ssize_t read_from_socket();
    
    /**
     * @brief Записать данные в сокет
     * @return количество записанных байт, -1 если ошибка
     */
    [[nodiscard]] ssize_t write_to_socket();
    
    /**
     * @brief Попытаться распарсить complete сообщения из буфера
     */
    void try_parse_messages();
    
    /**
     * @brief Обработать parsed сообщение
     */
    void handle_message(const RequestMessage& msg);
    
    int client_fd_;
    std::shared_ptr<Logger> logger_;
    MessageHandler message_handler_;
    ErrorHandler error_handler_;
    
    SessionState state_{SessionState::Active};
    IOBuffer read_buffer_;
    IOBuffer write_buffer_;
    JsonProtocolParser parser_;
    
    static inline uint64_t next_session_id_{0};
    uint64_t session_id_;
    
    static constexpr size_t MAX_BUFFER_SIZE = 1024 * 1024; // 1 MiB
};

} // namespace demo_daemon::ipc
