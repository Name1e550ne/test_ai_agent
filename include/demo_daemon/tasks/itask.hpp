#pragma once

#include <string>
#include <stop_token>
#include <memory>
#include <chrono>

namespace demo_daemon::tasks {

/**
 * @brief Состояния фоновой задачи.
 */
enum class TaskState {
    Pending,    // Задача создана, но не запущена
    Running,    // Задача выполняется
    Stopping,   // Задача получает сигнал остановки
    Stopped,    // Задача остановлена
    Failed      // Задача завершилась с ошибкой
};

/**
 * @brief Преобразует состояние в строку.
 */
[[nodiscard]] std::string task_state_to_string(TaskState state);

/**
 * @brief Интерфейс фоновой задачи.
 * 
 * Задачи должны быть потокобезопасными и поддерживать cooperative cancellation
 * через std::stop_token.
 */
class ITask {
public:
    virtual ~ITask() = default;

    /**
     * @brief Уникальный идентификатор задачи.
     */
    [[nodiscard]] virtual std::string id() const = 0;

    /**
     * @brief Тип задачи (для фабрики).
     */
    [[nodiscard]] virtual std::string type() const = 0;

    /**
     * @brief Описание задачи.
     */
    [[nodiscard]] virtual std::string description() const = 0;

    /**
     * @brief Текущее состояние задачи.
     */
    [[nodiscard]] virtual TaskState state() const = 0;

    /**
     * @brief Запуск задачи.
     * 
     * @param stop_token Токен для проверки запроса на остановку.
     * @return true если задача запустилась успешно, false иначе.
     */
    [[nodiscard]] virtual bool start(std::stop_token stop_token) = 0;

    /**
     * @brief Запрос на остановку задачи.
     * 
     * Должен быть неблокирующим. Фактическая остановка происходит
     * внутри метода run() при проверке stop_token.
     */
    virtual void request_stop() = 0;

    /**
     * @brief Ожидание завершения задачи.
     * 
     * @param timeout Максимальное время ожидания. Если timeout <= 0, ждет бесконечно.
     * @return true если задача остановилась, false если таймаут.
     */
    [[nodiscard]] virtual bool wait_stopped(std::chrono::milliseconds timeout) = 0;

    /**
     * @brief Получение последней ошибки задачи.
     */
    [[nodiscard]] virtual std::string last_error() const = 0;
};

} // namespace demo_daemon::tasks
