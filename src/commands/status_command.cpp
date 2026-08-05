#include "demo_daemon/commands/status_command.hpp"

namespace demo_daemon::commands {

core::CommandResult StatusCommand::execute(const core::CommandContext& context, const core::Json& params) {
    (void)params;
    
    core::Json result = {
        {"version", "0.1.0"},
        {"status", "running"}
    };
    
    if (context.task_manager) {
        result["active_tasks"] = context.task_manager->active_count();
    } else {
        result["active_tasks"] = 0;
    }
    
    return core::CommandResult::ok(std::move(result));
}

} // namespace demo_daemon::commands
