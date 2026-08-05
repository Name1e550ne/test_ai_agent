#include "demo_daemon/cli/client.hpp"
#include "demo_daemon/core/logger.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <stdexcept>
#include <sstream>

namespace demo_daemon::cli {

struct DemoDaemonClient::Impl {
    std::string socket_path;
    std::chrono::milliseconds timeout;
    int fd = -1;
    uint64_t request_id = 0;
    
    Impl(std::string path, std::chrono::milliseconds to)
        : socket_path(std::move(path)), timeout(to) {}
    
    ~Impl() {
        if (fd >= 0) {
            ::close(fd);
        }
    }
    
    [[nodiscard]] bool setupSocket() {
        fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            return false;
        }
        
        // Устанавливаем таймаут
        struct timeval tv;
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(timeout).count();
        auto usec = std::chrono::duration_cast<std::chrono::microseconds>(
            timeout - std::chrono::seconds(sec)).count();
        tv.tv_sec = sec;
        tv.tv_usec = usec;
        
        if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0 ||
            ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
            ::close(fd);
            fd = -1;
            return false;
        }
        
        return true;
    }
    
    [[nodiscard]] bool doConnect() {
        if (!setupSocket()) {
            return false;
        }
        
        struct sockaddr_un addr {};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
        
        if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            ::close(fd);
            fd = -1;
            return false;
        }
        
        return true;
    }
    
    [[nodiscard]] std::string sendMessage(const nlohmann::json& message) {
        std::string json_str = message.dump() + "\n";
        
        // Отправка
        size_t total_sent = 0;
        while (total_sent < json_str.size()) {
            ssize_t sent = ::send(fd, json_str.data() + total_sent, 
                                  json_str.size() - total_sent, MSG_NOSIGNAL);
            if (sent < 0) {
                if (errno == EINTR) continue;
                throw std::runtime_error("Failed to send: " + std::string(std::strerror(errno)));
            }
            if (sent == 0) {
                throw std::runtime_error("Connection closed during send");
            }
            total_sent += static_cast<size_t>(sent);
        }
        
        // Чтение ответа
        std::string response;
        char buffer[4096];
        while (true) {
            ssize_t received = ::recv(fd, buffer, sizeof(buffer) - 1, 0);
            if (received < 0) {
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    throw std::runtime_error("Response timeout");
                }
                throw std::runtime_error("Failed to receive: " + std::string(std::strerror(errno)));
            }
            if (received == 0) {
                if (response.empty()) {
                    throw std::runtime_error("Connection closed unexpectedly");
                }
                break;
            }
            
            response.append(buffer, static_cast<size_t>(received));
            
            // Проверяем есть ли полный JSON (заканчивается на newline)
            if (!response.empty() && response.back() == '\n') {
                break;
            }
        }
        
        // Удаляем trailing newline
        if (!response.empty() && response.back() == '\n') {
            response.pop_back();
        }
        
        return response;
    }
};

DemoDaemonClient::DemoDaemonClient(std::string socket_path, 
                                   std::chrono::milliseconds timeout)
    : pimpl_(std::make_unique<Impl>(std::move(socket_path), timeout)) {
}

DemoDaemonClient::~DemoDaemonClient() = default;

DemoDaemonClient::DemoDaemonClient(DemoDaemonClient&&) noexcept = default;
DemoDaemonClient& DemoDaemonClient::operator=(DemoDaemonClient&&) noexcept = default;

bool DemoDaemonClient::connect() {
    return pimpl_->doConnect();
}

void DemoDaemonClient::disconnect() {
    if (pimpl_->fd >= 0) {
        ::close(pimpl_->fd);
        pimpl_->fd = -1;
    }
}

bool DemoDaemonClient::isConnected() const {
    return pimpl_->fd >= 0;
}

CommandResponse DemoDaemonClient::sendRequest(const std::string& method, 
                                               const nlohmann::json& params) {
    auto start = std::chrono::steady_clock::now();
    
    try {
        // Формируем запрос
        nlohmann::json request = {
            {"id", ++pimpl_->request_id},
            {"method", method},
            {"params", params}
        };
        
        // Отправляем и получаем ответ
        std::string response_str = pimpl_->sendMessage(request);
        
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // Парсим ответ
        nlohmann::json response = nlohmann::json::parse(response_str);
        
        CommandResponse result;
        result.response_time = duration;
        
        if (response.contains("error")) {
            result.success = false;
            result.error_code = response["error"].value("code", -1);
            result.message = response["error"].value("message", "Unknown error");
            result.data = response["error"].value("data", nlohmann::json::object());
        } else if (response.contains("result")) {
            result.success = true;
            result.error_code = std::nullopt;
            result.message = "OK";
            result.data = response["result"];
        } else {
            result.success = false;
            result.error_code = -32600;
            result.message = "Invalid response format";
            result.data = nlohmann::json::object();
        }
        
        return result;
        
    } catch (const std::exception& e) {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        return CommandResponse{
            .success = false,
            .error_code = -1,
            .message = std::string("Communication error: ") + e.what(),
            .data = nlohmann::json::object(),
            .response_time = duration
        };
    }
}

} // namespace demo_daemon::cli
