#include "demo_daemon/core/command_registry.hpp"

namespace demo_daemon::core {

void CommandRegistry::register_command(std::shared_ptr<ICommand> cmd) {
    if (!cmd) {
        throw std::invalid_argument("Cannot register null command");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    commands_[cmd->name()] = std::move(cmd);
}

std::optional<std::shared_ptr<ICommand>> CommandRegistry::get_command(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = commands_.find(name);
    if (it != commands_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::string> CommandRegistry::list_commands() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    result.reserve(commands_.size());
    for (const auto& [name, _] : commands_) {
        result.push_back(name);
    }
    return result;
}

} // namespace demo_daemon::core
