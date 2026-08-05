#include "demo_daemon/commands/commands_list_command.hpp"

namespace demo_daemon::commands {

core::CommandResult CommandsListCommand::execute(const core::CommandContext& context, const core::Json& params) {
    (void)context;
    (void)params;
    
    // Эта команда должна получить доступ к реестру команд через контекст сервиса
    // Для простоты пока возвращаем заглушку
    core::Json result = {
        {"commands", core::Json::array({
            {{"name", "ping"}, {"description", "Simple ping command"}},
            {{"name", "shutdown"}, {"description", "Gracefully shutdown the daemon"}},
            {{"name", "commands.list"}, {"description", "List all available commands"}}
        })}
    };
    
    return core::CommandResult::ok(std::move(result));
}

} // namespace demo_daemon::commands
