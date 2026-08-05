#include "demo_daemon/commands/tasks_list_command.hpp"

namespace demo_daemon::commands {

TasksListCommand::TasksListCommand(tasks::TaskManager& task_manager)
    : task_manager_(task_manager) {}

core::CommandResult TasksListCommand::execute(const core::CommandContext& context, const core::Json& params) {
    (void)context;
    (void)params;
    
    auto tasks = task_manager_.list_tasks();
    
    core::Json tasks_array = core::Json::array();
    for (const auto& task : tasks) {
        tasks_array.push_back({
            {"id", task.id},
            {"type", task.type},
            {"state", task.state == tasks::TaskState::Running ? "running" : 
                       task.state == tasks::TaskState::Stopped ? "stopped" :
                       task.state == tasks::TaskState::Stopping ? "stopping" :
                       task.state == tasks::TaskState::Failed ? "failed" : "pending"}
        });
    }
    
    core::Json result = {
        {"tasks", tasks_array},
        {"count", static_cast<int>(tasks.size())}
    };
    
    return core::CommandResult::ok(std::move(result));
}

} // namespace demo_daemon::commands
