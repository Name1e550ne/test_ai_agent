#pragma once

#include "demo_daemon/core/command.hpp"
#include <string>

namespace demo_daemon::commands {

class CommandsListCommand : public core::ICommand {
public:
    [[nodiscard]] std::string name() const override { return "commands.list"; }
    [[nodiscard]] std::string description() const override { return "List all available commands"; }
    
    [[nodiscard]] core::CommandResult execute(const core::CommandContext& context, const core::Json& params) override;
};

} // namespace demo_daemon::commands
