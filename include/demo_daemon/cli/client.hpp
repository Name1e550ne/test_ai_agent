#pragma once

#include <string>
#include <chrono>
#include <optional>
#include <memory>
#include <nlohmann/json.hpp>

namespace demo_daemon::cli {

/**
 * @brief Результат выполнения команды CLI
 */
struct CommandResponse {
    bool success;
    std::optional<int> error_code;
    std::string message;
    nlohmann::json data;
    std::chrono::milliseconds response_time;
};

/**
 * @brief Клиент для взаимодействия с демоном через Unix Domain Socket
 */
class DemoDaemonClient {
public:
    /**
     * @brief Конструктор клиента
     * @param socket_path Путь к Unix Domain Socket
     * @param timeout Таймаут подключения и операций в миллисекундах
     */
    explicit DemoDaemonClient(std::string socket_path, 
                              std::chrono::milliseconds timeout = std::chrono::seconds(5));
    
    ~DemoDaemonClient();
    
    // Запрет копирования
    DemoDaemonClient(const DemoDaemonClient&) = delete;
    DemoDaemonClient& operator=(const DemoDaemonClient&) = delete;
    
    // Разрешение перемещения
    DemoDaemonClient(DemoDaemonClient&&) noexcept;
    DemoDaemonClient& operator=(DemoDaemonClient&&) noexcept;

    /**
     * @brief Подключение к демону
     * @return true если подключение успешно
     */
    [[nodiscard]] bool connect();
    
    /**
     * @brief Отключение от демона
     */
    void disconnect();
    
    /**
     * @brief Проверка подключения
     * @return true если подключен
     */
    [[nodiscard]] bool isConnected() const;
    
    /**
     * @brief Отправка запроса и получение ответа
     * @param method Имя метода
     * @param params Параметры запроса
     * @return Результат выполнения команды
     */
    [[nodiscard]] CommandResponse sendRequest(const std::string& method, 
                                               const nlohmann::json& params = {});

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace demo_daemon::cli
