#include "demo_daemon/commands/tasks_add_command.hpp"

namespace demo_daemon::commands {

TasksAddCommand::TasksAddCommand(tasks::TaskManager& task_manager)
    : task_manager_(task_manager) {}

core::CommandResult TasksAddCommand::execute(const core::CommandContext& context, const core::Json& params) {
    (void)context;
    
    if (!params.contains("type")) {
        return core::CommandResult::err(-32602, "Missing required parameter: type");
    }
    
    std::string task_type = params["type"].get<std::string>();
    
    auto task_id = task_manager_.add_task(task_type, params);
    
    if (task_id.empty()) {
        return core::CommandResult::err(-32000, "Failed to add task: unknown type or invalid params");
    }
    
    core::Json result = {
        {"id", task_id},
        {"type", task_type},
        {"status", "started"}
    };
    
    return core::CommandResult::ok(std::move(result));
}

} // namespace demo_daemon::commands
