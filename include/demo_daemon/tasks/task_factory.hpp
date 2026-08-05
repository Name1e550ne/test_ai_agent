#pragma once

#include "tasks/itask.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <vector>

namespace demo_daemon::tasks {

/**
 * @brief Factory for creating tasks by type name.
 * 
 * Provides extensibility point for adding new task types without modifying core code.
 */
class TaskFactory {
public:
    using CreatorFunc = std::function<std::unique_ptr<ITask>(const nlohmann::json&)>;

    /**
     * @brief Register a task type with its creator function.
     * @param type_name Unique type identifier
     * @param creator Function that creates task instance from parameters
     * @return true if registered successfully, false if type already exists
     */
    [[nodiscard]] bool registerTask(const std::string& type_name, CreatorFunc creator);

    /**
     * @brief Create a task instance by type name.
     * @param type_name Type of task to create
     * @param params Parameters for task construction
     * @return Created task or nullptr if type not found
     */
    [[nodiscard]] std::unique_ptr<ITask> createTask(
        const std::string& type_name, 
        const nlohmann::json& params) const;

    /**
     * @brief List all registered task types.
     * @return Vector of type names
     */
    [[nodiscard]] std::vector<std::string> getRegisteredTypes() const;

    /**
     * @brief Get singleton instance.
     */
    static TaskFactory& instance();

private:
    TaskFactory() = default;
    ~TaskFactory() = default;
    
    // Non-copyable, non-movable
    TaskFactory(const TaskFactory&) = delete;
    TaskFactory& operator=(const TaskFactory&) = delete;
    TaskFactory(TaskFactory&&) = delete;
    TaskFactory& operator=(TaskFactory&&) = delete;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, CreatorFunc> creators_;
};

} // namespace demo_daemon::tasks
