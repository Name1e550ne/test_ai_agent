#pragma once

#include "demo_daemon/tasks/itask.hpp"
#include "demo_daemon/tasks/task_registry.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <stop_token>
#include <thread>
#include <chrono>

namespace demo_daemon::tasks {

/**
 * @brief Информация о задаче в менеджере.
 */
struct TaskInfo {
    std::string id;
    std::string type;
    std::string description;
    TaskState state;
    std::string last_error;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point started_at;
    std::chrono::steady_clock::time_point stopped_at;
};

/**
 * @brief Менеджер фоновых задач.
 * 
 * Управляет жизненным циклом задач:
 * - Создание задач через registry
 * - Запуск задач в отдельных потоках
 * - Остановка задач (graceful shutdown)
 * - Отслеживание состояния
 * - Очистка завершенных задач
 * 
 * Потокобезопасен.
 */
class TaskManager {
public:
    explicit TaskManager(std::shared_ptr<TaskRegistry> registry);
    ~TaskManager();

    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;
    TaskManager(TaskManager&&) = delete;
    TaskManager& operator=(TaskManager&&) = delete;

    /**
     * @brief Добавление и запуск задачи.
     * 
     * @param type Тип задачи (должен быть зарегистрирован).
     * @param params Параметры для инициализации задачи.
     * @return ID созданной задачи или пустая строка при ошибке.
     */
    [[nodiscard]] std::string add_task(const std::string& type, const nlohmann::json& params);

    /**
     * @brief Остановка задачи по ID.
     * 
     * @param task_id ID задачи.
     * @return true если задача найдена и остановка запрошена, false иначе.
     */
    [[nodiscard]] bool stop_task(const std::string& task_id);

    /**
     * @brief Остановка всех задач.
     * 
     * Блокирует вызывающий поток до завершения всех задач.
     */
    void stop_all_tasks();

    /**
     * @brief Получение информации о задаче.
     * 
     * @param task_id ID задачи.
     * @return TaskInfo или std::nullopt если задача не найдена.
     */
    [[nodiscard]] std::optional<TaskInfo> get_task_info(const std::string& task_id) const;

    /**
     * @brief Список всех задач.
     * 
     * @return Вектор с информацией обо всех задачах.
     */
    [[nodiscard]] std::vector<TaskInfo> list_tasks() const;

    /**
     * @brief Количество активных задач (Running + Stopping).
     */
    [[nodiscard]] std::size_t active_count() const;

    /**
     * @brief Общее количество задач.
     */
    [[nodiscard]] std::size_t total_count() const;

    /**
     * @brief Реестр задач.
     */
    [[nodiscard]] std::shared_ptr<TaskRegistry> registry() const { return registry_; }

private:
    struct TaskEntry {
        std::unique_ptr<ITask> task;
        std::thread thread;
        std::stop_source stop_source;
        std::atomic<bool> running{false};
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point started_at;
        std::chrono::steady_clock::time_point stopped_at;
        
        TaskEntry() = default;
        
        // Запрещаем копирование, разрешаем только перемещение с явной реализацией
        TaskEntry(const TaskEntry&) = delete;
        TaskEntry& operator=(const TaskEntry&) = delete;
        
        TaskEntry(TaskEntry&& other) noexcept
            : task(std::move(other.task))
            , thread(std::move(other.thread))
            , stop_source(std::move(other.stop_source))
            , running(other.running.load(std::memory_order_relaxed))
            , created_at(other.created_at)
            , started_at(other.started_at)
            , stopped_at(other.stopped_at)
        {}
        
        TaskEntry& operator=(TaskEntry&& other) noexcept {
            if (this != &other) {
                task = std::move(other.task);
                thread = std::move(other.thread);
                stop_source = std::move(other.stop_source);
                running.store(other.running.load(std::memory_order_relaxed), std::memory_order_relaxed);
                created_at = other.created_at;
                started_at = other.started_at;
                stopped_at = other.stopped_at;
            }
            return *this;
        }
    };

    void run_task(TaskEntry& entry, std::stop_token token);

    mutable std::mutex mutex_;
    std::shared_ptr<TaskRegistry> registry_;
    std::unordered_map<std::string, TaskEntry> tasks_;
    std::atomic<std::size_t> next_task_id_{0};
    std::atomic<bool> shutdown_requested_{false};
};

} // namespace demo_daemon::tasks
