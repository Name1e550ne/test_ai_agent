#pragma once

#include "demo_daemon/core/command.hpp"
#include <string>

namespace demo_daemon::commands {

class ShutdownCommand : public core::ICommand {
public:
    [[nodiscard]] std::string name() const override { return "shutdown"; }
    [[nodiscard]] std::string description() const override { return "Gracefully shutdown the daemon"; }
    
    [[nodiscard]] core::CommandResult execute(const core::CommandContext& context, const core::Json& params) override;
};

} // namespace demo_daemon::commands
