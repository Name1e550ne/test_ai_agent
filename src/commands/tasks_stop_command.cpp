#include "demo_daemon/commands/tasks_stop_command.hpp"

namespace demo_daemon::commands {

TasksStopCommand::TasksStopCommand(tasks::TaskManager& task_manager)
    : task_manager_(task_manager) {}

core::CommandResult TasksStopCommand::execute(const core::CommandContext& context, const core::Json& params) {
    (void)context;
    
    if (!params.contains("id")) {
        return core::CommandResult::err(-32602, "Missing required parameter: id");
    }
    
    std::string task_id = params["id"].get<std::string>();
    
    if (!task_manager_.stop_task(task_id)) {
        return core::CommandResult::err(-32001, "Task not found or already stopped: " + task_id);
    }
    
    core::Json result = {
        {"id", task_id},
        {"status", "stopping"}
    };
    
    return core::CommandResult::ok(std::move(result));
}

} // namespace demo_daemon::commands
