#pragma once

#include "demo_daemon/core/command.hpp"
#include <string>

namespace demo_daemon::commands {

class PingCommand : public core::ICommand {
public:
    [[nodiscard]] std::string name() const override { return "ping"; }
    [[nodiscard]] std::string description() const override { return "Simple ping command to check connectivity"; }
    
    [[nodiscard]] core::CommandResult execute(const core::CommandContext& context, const core::Json& params) override;
};

} // namespace demo_daemon::commands
