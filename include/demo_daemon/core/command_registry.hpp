#pragma once

#include "demo_daemon/core/command.hpp"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <string>
#include <optional>

namespace demo_daemon::core {

class CommandRegistry {
public:
    void register_command(std::shared_ptr<ICommand> cmd);
    [[nodiscard]] std::optional<std::shared_ptr<ICommand>> get_command(const std::string& name) const;
    [[nodiscard]] std::vector<std::string> list_commands() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<ICommand>> commands_;
};

} // namespace demo_daemon::core
