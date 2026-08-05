#include "demo_daemon/ipc/json_protocol.hpp"
#include <sstream>

namespace demo_daemon::ipc {

std::string protocol_error_code_to_string(ProtocolErrorCode code) {
    switch (code) {
        case ProtocolErrorCode::ParseError:
            return "Parse error";
        case ProtocolErrorCode::InvalidRequest:
            return "Invalid Request";
        case ProtocolErrorCode::MethodNotFound:
            return "Method not found";
        case ProtocolErrorCode::InvalidParams:
            return "Invalid params";
        case ProtocolErrorCode::InternalError:
            return "Internal error";
        case ProtocolErrorCode::ServerBusy:
            return "Server busy";
        case ProtocolErrorCode::Timeout:
            return "Timeout";
        case ProtocolErrorCode::NotFound:
            return "Not found";
        case ProtocolErrorCode::Unauthorized:
            return "Unauthorized";
        case ProtocolErrorCode::Forbidden:
            return "Forbidden";
        default:
            return "Unknown error";
    }
}

// ============================================================================
// ProtocolError implementation
// ============================================================================

nlohmann::json ProtocolError::to_json() const {
    nlohmann::json j = {
        {"code", static_cast<int32_t>(code)},
        {"message", message}
    };
    if (data.has_value()) {
        j["data"] = data.value();
    }
    return j;
}

ProtocolError ProtocolError::from_json(const nlohmann::json& j) {
    ProtocolError err;
    err.code = static_cast<ProtocolErrorCode>(j.value("code", -32603));
    err.message = j.value("message", std::string("Unknown error"));
    if (j.contains("data")) {
        err.data = j["data"];
    }
    return err;
}

// ============================================================================
// RequestMessage implementation
// ============================================================================

nlohmann::json RequestMessage::to_json() const {
    nlohmann::json j = {
        {"method", method},
        {"params", params}
    };
    if (id.has_value()) {
        j["id"] = id.value();
    }
    return j;
}

std::optional<RequestMessage> RequestMessage::from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        return std::nullopt;
    }
    
    if (!j.contains("method") || !j["method"].is_string()) {
        return std::nullopt;
    }
    
    RequestMessage msg;
    msg.method = j["method"].get<std::string>();
    
    if (j.contains("id")) {
        if (j["id"].is_number_integer()) {
            msg.id = j["id"].get<int64_t>();
        } else if (!j["id"].is_null()) {
            // ID must be integer or null
            return std::nullopt;
        }
    }
    
    if (j.contains("params")) {
        msg.params = j["params"];
    } else {
        msg.params = nlohmann::json::object();
    }
    
    return msg;
}

// ============================================================================
// ResponseMessage implementation
// ============================================================================

nlohmann::json ResponseMessage::to_json() const {
    nlohmann::json j = {
        {"result", result}
    };
    if (id.has_value()) {
        j["id"] = id.value();
    }
    return j;
}

std::optional<ResponseMessage> ResponseMessage::from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        return std::nullopt;
    }
    
    if (!j.contains("result")) {
        return std::nullopt;
    }
    
    ResponseMessage msg;
    msg.result = j["result"];
    
    if (j.contains("id")) {
        if (j["id"].is_number_integer()) {
            msg.id = j["id"].get<int64_t>();
        } else if (!j["id"].is_null()) {
            return std::nullopt;
        }
    }
    
    return msg;
}

// ============================================================================
// ErrorResponseMessage implementation
// ============================================================================

nlohmann::json ErrorResponseMessage::to_json() const {
    nlohmann::json j = {
        {"error", error.to_json()}
    };
    if (id.has_value()) {
        j["id"] = id.value();
    }
    return j;
}

std::optional<ErrorResponseMessage> ErrorResponseMessage::from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        return std::nullopt;
    }
    
    if (!j.contains("error") || !j["error"].is_object()) {
        return std::nullopt;
    }
    
    ErrorResponseMessage msg;
    msg.error = ProtocolError::from_json(j["error"]);
    
    if (j.contains("id")) {
        if (j["id"].is_number_integer()) {
            msg.id = j["id"].get<int64_t>();
        } else if (!j["id"].is_null()) {
            return std::nullopt;
        }
    }
    
    return msg;
}

// ============================================================================
// JsonMessage implementation
// ============================================================================

nlohmann::json JsonMessage::to_json() const {
    nlohmann::json j;
    
    if (id.has_value()) {
        j["id"] = id.value();
    }
    
    if (method.has_value()) {
        j["method"] = method.value();
    }
    
    if (params.has_value()) {
        j["params"] = params.value();
    }
    
    if (result.has_value()) {
        j["result"] = result.value();
    }
    
    if (error.has_value()) {
        j["error"] = error.value().to_json();
    }
    
    return j;
}

std::optional<JsonMessage> JsonMessage::from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        return std::nullopt;
    }
    
    JsonMessage msg;
    
    if (j.contains("id")) {
        if (j["id"].is_number_integer()) {
            msg.id = j["id"].get<int64_t>();
        } else if (!j["id"].is_null()) {
            return std::nullopt;
        }
    }
    
    if (j.contains("method")) {
        if (!j["method"].is_string()) {
            return std::nullopt;
        }
        msg.method = j["method"].get<std::string>();
    }
    
    if (j.contains("params")) {
        msg.params = j["params"];
    }
    
    if (j.contains("result")) {
        msg.result = j["result"];
    }
    
    if (j.contains("error")) {
        if (!j["error"].is_object()) {
            return std::nullopt;
        }
        msg.error = ProtocolError::from_json(j["error"]);
    }
    
    // Validate: must be either request or response, not both
    bool has_method = msg.method.has_value();
    bool has_result = msg.result.has_value();
    bool has_error = msg.error.has_value();
    
    if (has_method && (has_result || has_error)) {
        // Cannot have both method and result/error
        return std::nullopt;
    }
    
    if (!has_method && !has_result && !has_error) {
        // Must have at least one
        return std::nullopt;
    }
    
    return msg;
}

// ============================================================================
// JsonProtocolParser implementation
// ============================================================================

JsonProtocolParser::JsonProtocolParser(size_t max_message_size)
    : max_message_size_(max_message_size) {}

std::optional<JsonMessage> JsonProtocolParser::try_parse(std::string_view data, size_t& bytes_consumed) {
    bytes_consumed = 0;
    
    if (data.empty()) {
        return std::nullopt;
    }
    
    // Find newline delimiter
    auto newline_pos = data.find('\n');
    if (newline_pos == std::string_view::npos) {
        // No complete message yet
        if (data.size() >= max_message_size_) {
            // Message too large, discard and return parse error
            bytes_consumed = data.size();
            return make_parse_error();
        }
        return std::nullopt;
    }
    
    // Check message size limit
    if (newline_pos >= max_message_size_) {
        bytes_consumed = newline_pos + 1;
        return make_parse_error();
    }
    
    // Extract line (without newline)
    std::string_view line = data.substr(0, newline_pos);
    bytes_consumed = newline_pos + 1;
    
    // Trim whitespace
    auto start = line.find_first_not_of(" \t\r");
    auto end = line.find_last_not_of(" \t\r");
    
    if (start == std::string_view::npos) {
        // Empty line, skip
        return std::nullopt;
    }
    
    line = line.substr(start, end - start + 1);
    
    // Parse JSON
    try {
        auto json = nlohmann::json::parse(line);
        return JsonMessage::from_json(json);
    } catch (const nlohmann::json::parse_error&) {
        return make_parse_error();
    } catch (...) {
        return make_parse_error();
    }
}

std::string JsonProtocolParser::serialize(const JsonMessage& msg) const {
    auto json = msg.to_json();
    std::string serialized = json.dump();
    serialized += '\n';
    return serialized;
}

JsonMessage JsonProtocolParser::make_parse_error() {
    JsonMessage msg;
    msg.error = ProtocolError{
        ProtocolErrorCode::ParseError,
        "Invalid JSON payload",
        std::nullopt
    };
    return msg;
}

JsonMessage JsonProtocolParser::make_invalid_request(int64_t id) {
    JsonMessage msg;
    msg.id = id;
    msg.error = ProtocolError{
        ProtocolErrorCode::InvalidRequest,
        "The JSON sent is not a valid Request object",
        std::nullopt
    };
    return msg;
}

JsonMessage JsonProtocolParser::make_method_not_found(int64_t id, std::string_view method) {
    JsonMessage msg;
    msg.id = id;
    msg.error = ProtocolError{
        ProtocolErrorCode::MethodNotFound,
        std::string("Method not found: ") + std::string(method),
        std::nullopt
    };
    return msg;
}

JsonMessage JsonProtocolParser::make_invalid_params(int64_t id, std::string_view message) {
    JsonMessage msg;
    msg.id = id;
    msg.error = ProtocolError{
        ProtocolErrorCode::InvalidParams,
        std::string(message),
        std::nullopt
    };
    return msg;
}

JsonMessage JsonProtocolParser::make_internal_error(int64_t id, std::string_view message) {
    JsonMessage msg;
    msg.id = id;
    msg.error = ProtocolError{
        ProtocolErrorCode::InternalError,
        std::string(message),
        std::nullopt
    };
    return msg;
}

} // namespace demo_daemon::ipc
