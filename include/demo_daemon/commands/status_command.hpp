#pragma once

#include "demo_daemon/core/command.hpp"
#include "demo_daemon/tasks/task_manager.hpp"
#include <string>
#include <memory>

namespace demo_daemon::commands {

class StatusCommand : public core::ICommand {
public:
    [[nodiscard]] std::string name() const override { return "status"; }
    [[nodiscard]] std::string description() const override { return "Get daemon status information"; }
    
    [[nodiscard]] core::CommandResult execute(const core::CommandContext& context, const core::Json& params) override;
};

} // namespace demo_daemon::commands
