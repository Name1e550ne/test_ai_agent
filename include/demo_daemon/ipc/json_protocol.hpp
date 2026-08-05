#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace demo_daemon::ipc {

/**
 * @brief Protocol error codes (JSON-RPC like)
 */
enum class ProtocolErrorCode : int32_t {
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,
    
    // Application-specific errors (reserved range)
    ServerBusy = -32000,
    Timeout = -32001,
    NotFound = -32002,
    Unauthorized = -32003,
    Forbidden = -32004
};

/**
 * @brief Convert error code to string message
 */
[[nodiscard]] std::string protocol_error_code_to_string(ProtocolErrorCode code);

/**
 * @brief Protocol Error object
 */
struct ProtocolError {
    ProtocolErrorCode code;
    std::string message;
    std::optional<nlohmann::json> data;
    
    [[nodiscard]] nlohmann::json to_json() const;
    [[nodiscard]] static ProtocolError from_json(const nlohmann::json& j);
};

/**
 * @brief Request message structure
 */
struct RequestMessage {
    std::optional<int64_t> id;  // null for notifications
    std::string method;
    nlohmann::json params;
    
    [[nodiscard]] nlohmann::json to_json() const;
    [[nodiscard]] static std::optional<RequestMessage> from_json(const nlohmann::json& j);
};

/**
 * @brief Response message structure (success)
 */
struct ResponseMessage {
    std::optional<int64_t> id;
    nlohmann::json result;
    
    [[nodiscard]] nlohmann::json to_json() const;
    [[nodiscard]] static std::optional<ResponseMessage> from_json(const nlohmann::json& j);
};

/**
 * @brief Response message structure (error)
 */
struct ErrorResponseMessage {
    std::optional<int64_t> id;
    ProtocolError error;
    
    [[nodiscard]] nlohmann::json to_json() const;
    [[nodiscard]] static std::optional<ErrorResponseMessage> from_json(const nlohmann::json& j);
};

/**
 * @brief Generic message that can be request or response
 */
struct JsonMessage {
    std::optional<int64_t> id;
    std::optional<std::string> method;
    std::optional<nlohmann::json> params;
    std::optional<nlohmann::json> result;
    std::optional<ProtocolError> error;
    
    [[nodiscard]] bool is_request() const { return method.has_value(); }
    [[nodiscard]] bool is_response() const { return result.has_value() || error.has_value(); }
    [[nodiscard]] bool is_notification() const { return !id.has_value() && method.has_value(); }
    [[nodiscard]] bool is_error_response() const { return error.has_value(); }
    
    [[nodiscard]] nlohmann::json to_json() const;
    [[nodiscard]] static std::optional<JsonMessage> from_json(const nlohmann::json& j);
};

/**
 * @brief Newline-delimited JSON protocol parser/serializer
 * 
 * Handles framing: each message is a single line terminated by '\n'
 * Maximum message size is enforced to prevent DoS
 */
class JsonProtocolParser {
public:
    static constexpr size_t DEFAULT_MAX_MESSAGE_SIZE = 1024 * 1024; // 1 MiB
    
    explicit JsonProtocolParser(size_t max_message_size = DEFAULT_MAX_MESSAGE_SIZE);
    
    /**
     * @brief Try to parse a complete message from buffer
     * @param data Input data buffer
     * @param bytes_consumed Output: number of bytes consumed from buffer (including '\n')
     * @return Parsed message if complete, std::nullopt if more data needed
     */
    [[nodiscard]] std::optional<JsonMessage> try_parse(std::string_view data, size_t& bytes_consumed);
    
    /**
     * @brief Serialize message to newline-delimited JSON string
     */
    [[nodiscard]] std::string serialize(const JsonMessage& msg) const;
    
    /**
     * @brief Create error response for parse errors
     */
    [[nodiscard]] static JsonMessage make_parse_error();
    
    /**
     * @brief Create error response for invalid request
     */
    [[nodiscard]] static JsonMessage make_invalid_request(int64_t id);
    
    /**
     * @brief Create error response for unknown method
     */
    [[nodiscard]] static JsonMessage make_method_not_found(int64_t id, std::string_view method);
    
    /**
     * @brief Create error response for invalid params
     */
    [[nodiscard]] static JsonMessage make_invalid_params(int64_t id, std::string_view message);
    
    /**
     * @brief Create error response for internal error
     */
    [[nodiscard]] static JsonMessage make_internal_error(int64_t id, std::string_view message);
    
private:
    size_t max_message_size_;
};

} // namespace demo_daemon::ipc
