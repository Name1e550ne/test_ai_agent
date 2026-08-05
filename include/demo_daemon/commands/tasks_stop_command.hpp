#pragma once

#include "demo_daemon/core/command.hpp"
#include "demo_daemon/tasks/task_manager.hpp"
#include <string>
#include <memory>

namespace demo_daemon::commands {

class TasksStopCommand : public core::ICommand {
public:
    explicit TasksStopCommand(tasks::TaskManager& task_manager);
    
    [[nodiscard]] std::string name() const override { return "tasks.stop"; }
    [[nodiscard]] std::string description() const override { return "Stop a task"; }
    
    [[nodiscard]] core::CommandResult execute(const core::CommandContext& context, const core::Json& params) override;

private:
    tasks::TaskManager& task_manager_;
};

} // namespace demo_daemon::commands
