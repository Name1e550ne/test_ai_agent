#include "demo_daemon/commands/shutdown_command.hpp"

namespace demo_daemon::commands {

core::CommandResult ShutdownCommand::execute(const core::CommandContext& context, const core::Json& params) {
    (void)context;
    (void)params;
    
    // В реальном приложении здесь был бы сигнал к остановке основного цикла
    // Но сама команда только подтверждает запрос
    core::Json result = {
        {"status", "shutdown_initiated"}
    };
    
    return core::CommandResult::ok(std::move(result));
}

} // namespace demo_daemon::commands
