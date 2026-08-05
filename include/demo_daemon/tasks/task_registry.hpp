#pragma once

#include "demo_daemon/tasks/itask.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <memory>
#include <functional>

namespace demo_daemon::tasks {

/**
 * @brief Фабричный тип для создания задач.
 */
using TaskFactory = std::function<std::unique_ptr<ITask>(const std::string& id, const nlohmann::json& params)>;

/**
 * @brief Реестр фабрик задач.
 * 
 * Позволяет регистрировать и создавать задачи по типу.
 * Потокобезопасен.
 */
class TaskRegistry {
public:
    TaskRegistry() = default;
    ~TaskRegistry() = default;

    TaskRegistry(const TaskRegistry&) = delete;
    TaskRegistry& operator=(const TaskRegistry&) = delete;
    TaskRegistry(TaskRegistry&&) = delete;
    TaskRegistry& operator=(TaskRegistry&&) = delete;

    /**
     * @brief Регистрация фабрики задачи.
     * 
     * @param type Тип задачи (уникальное имя).
     * @param factory Фабрика для создания экземпляров.
     * @return true если регистрация успешна, false если тип уже занят.
     */
    [[nodiscard]] bool register_task_type(const std::string& type, TaskFactory factory);

    /**
     * @brief Создание задачи.
     * 
     * @param type Тип задачи.
     * @param id Уникальный идентификатор экземпляра.
     * @param params Параметры для инициализации.
     * @return unique_ptr к задаче или nullptr если тип не найден.
     */
    [[nodiscard]] std::unique_ptr<ITask> create_task(
        const std::string& type,
        const std::string& id,
        const nlohmann::json& params);

    /**
     * @brief Список зарегистрированных типов задач.
     */
    [[nodiscard]] std::vector<std::string> get_registered_types() const;

    /**
     * @brief Проверка наличия типа.
     */
    [[nodiscard]] bool has_type(const std::string& type) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, TaskFactory> factories_;
};

} // namespace demo_daemon::tasks
