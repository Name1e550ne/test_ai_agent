#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace demo_daemon::core {

using Json = nlohmann::json;

struct CommandContext {
    // В будущем можно добавить информацию о клиенте, права доступа и т.д.
    std::string client_id;
};

struct CommandResult {
    bool success{true};
    int error_code{0};
    std::string message;
    Json data;

    static CommandResult ok(Json result_data = Json::object()) {
        return {true, 0, "OK", std::move(result_data)};
    }

    static CommandResult err(int code, const std::string& msg) {
        return {false, code, msg, Json::object()};
    }
};

class ICommand {
public:
    virtual ~ICommand() = default;

    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual std::string description() const = 0;
    [[nodiscard]] virtual CommandResult execute(const CommandContext& context, const Json& params) = 0;
};

} // namespace demo_daemon::core
