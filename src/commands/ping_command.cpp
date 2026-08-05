#include "demo_daemon/commands/ping_command.hpp"

namespace demo_daemon::commands {

core::CommandResult PingCommand::execute(const core::CommandContext& context, const core::Json& params) {
    (void)context;
    (void)params;
    
    core::Json result = {
        {"message", "pong"},
        {"timestamp", std::time(nullptr)}
    };
    
    return core::CommandResult::ok(std::move(result));
}

} // namespace demo_daemon::commands
