#pragma once

#include "demo_daemon/core/command.hpp"
#include "demo_daemon/tasks/task_manager.hpp"
#include <string>
#include <memory>

namespace demo_daemon::commands {

class TasksAddCommand : public core::ICommand {
public:
    explicit TasksAddCommand(tasks::TaskManager& task_manager);
    
    [[nodiscard]] std::string name() const override { return "tasks.add"; }
    [[nodiscard]] std::string description() const override { return "Add a new task"; }
    
    [[nodiscard]] core::CommandResult execute(const core::CommandContext& context, const core::Json& params) override;

private:
    tasks::TaskManager& task_manager_;
};

} // namespace demo_daemon::commands
