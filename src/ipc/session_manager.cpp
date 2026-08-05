#include "demo_daemon/ipc/session_manager.hpp"

#include <cstring>
#include <stdexcept>
#include <fcntl.h>

namespace demo_daemon::ipc {

Session::Session(int client_fd,
                 std::shared_ptr<Logger> logger,
                 MessageHandler message_handler,
                 ErrorHandler error_handler)
    : client_fd_(client_fd)
    , logger_(std::move(logger))
    , message_handler_(std::move(message_handler))
    , error_handler_(std::move(error_handler))
    , session_id_(++next_session_id_)
{
}

Session::~Session() {
    close();
    if (client_fd_ >= 0) {
        ::close(client_fd_);
        client_fd_ = -1;
    }
}

bool Session::initialize() {
    // Устанавливаем non-blocking mode
    int flags = ::fcntl(client_fd_, F_GETFL, 0);
    if (flags == -1) {
        logger_->error("Session", "Failed to get socket flags: {}", strerror(errno));
        return false;
    }
    
    if (::fcntl(client_fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
        logger_->error("Session", "Failed to set non-blocking mode: {}", strerror(errno));
        return false;
    }
    
    state_ = SessionState::Active;
    logger_->debug("Session", "Session {} initialized (fd={})", session_id_, client_fd_);
    return true;
}

bool Session::on_readable() {
    if (state_ != SessionState::Active && state_ != SessionState::Reading) {
        return false;
    }
    
    state_ = SessionState::Reading;
    
    while (true) {
        ssize_t bytes_read = read_from_socket();
        
        if (bytes_read > 0) {
            // Данные прочитаны, пробуем распарсить сообщения
            try_parse_messages();
        } else if (bytes_read == 0) {
            // EOF - клиент закрыл соединение
            logger_->debug("Session", "Session {} EOF (client disconnected)", session_id_);
            close();
            return false;
        } else {
            // Ошибка или EAGAIN/EWOULDBLOCK
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Нет данных для чтения сейчас, это нормально для non-blocking
                state_ = SessionState::Active;
                return true;
            }
            
            logger_->warn("Session", "Session {} read error: {}", session_id_, strerror(errno));
            close();
            return false;
        }
    }
}

bool Session::on_writable() {
    if (state_ != SessionState::Writing) {
        state_ = SessionState::Active;
        return true;
    }
    
    while (write_buffer_.available_for_read() > 0) {
        ssize_t bytes_written = write_to_socket();
        
        if (bytes_written > 0) {
            write_buffer_.advance_read(static_cast<size_t>(bytes_written));
            write_buffer_.compact();
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Софт буфер заполнен, ждем следующей готовности к записи
                return true;
            }
            
            logger_->warn("Session", "Session {} write error: {}", session_id_, strerror(errno));
            close();
            return false;
        }
    }
    
    // Буфер записан полностью
    state_ = SessionState::Active;
    return true;
}

void Session::schedule_send(const ResponseMessage& response) {
    JsonMessage msg;
    msg.id = response.id;
    msg.result = response.result;
    
    auto result = parser_.serialize(msg);
    if (result.empty()) {
        logger_->error("Session", "Failed to serialize response");
        return;
    }
    
    const std::string& json = result;
    
    // Добавляем newline delimiter
    write_buffer_.data.resize(write_buffer_.write_pos + json.size() + 1);
    std::memcpy(write_buffer_.get_write_ptr(), json.c_str(), json.size());
    write_buffer_.get_write_ptr()[json.size()] = '\n';
    write_buffer_.advance_write(json.size() + 1);
    
    logger_->trace("Session", "Scheduled response for session {}", session_id_);
}

void Session::schedule_send_error(const ErrorResponseMessage& error_response) {
    JsonMessage msg;
    msg.id = error_response.id;
    msg.error = error_response.error;
    
    auto result = parser_.serialize(msg);
    if (result.empty()) {
        logger_->error("Session", "Failed to serialize error response");
        return;
    }
    
    const std::string& json = result;
    
    // Добавляем newline delimiter
    write_buffer_.data.resize(write_buffer_.write_pos + json.size() + 1);
    std::memcpy(write_buffer_.get_write_ptr(), json.c_str(), json.size());
    write_buffer_.get_write_ptr()[json.size()] = '\n';
    write_buffer_.advance_write(json.size() + 1);
    
    logger_->trace("Session", "Scheduled error response for session {}", session_id_);
}

void Session::close() {
    if (state_ == SessionState::Closed || state_ == SessionState::Closing) {
        return;
    }
    
    state_ = SessionState::Closing;
    
    // Очищаем буферы
    read_buffer_.clear();
    write_buffer_.clear();
    
    state_ = SessionState::Closed;
    logger_->debug("Session", "Session {} closed", session_id_);
}

ssize_t Session::read_from_socket() {
    // Проверяем, не переполнен ли буфер
    if (read_buffer_.data.size() >= MAX_BUFFER_SIZE) {
        logger_->warn("Session", "Session {} buffer overflow, closing", session_id_);
        return -1;
    }
    
    // Compact буфер если нужно
    if (read_buffer_.read_pos > 0 && read_buffer_.available_for_read() > 0) {
        read_buffer_.compact();
    }
    
    // Расширяем буфер если нужно
    if (read_buffer_.available_for_write() < 1024) {
        size_t new_size = std::min(read_buffer_.data.size() * 2, MAX_BUFFER_SIZE);
        if (new_size > read_buffer_.data.size()) {
            read_buffer_.data.resize(new_size);
        }
    }
    
    ssize_t bytes_read = ::read(
        client_fd_,
        read_buffer_.get_write_ptr(),
        read_buffer_.available_for_write()
    );
    
    if (bytes_read > 0) {
        read_buffer_.advance_write(static_cast<size_t>(bytes_read));
    }
    
    return bytes_read;
}

ssize_t Session::write_to_socket() {
    if (write_buffer_.available_for_read() == 0) {
        return 0;
    }
    
    ssize_t bytes_written = ::write(
        client_fd_,
        write_buffer_.data.data() + write_buffer_.read_pos,
        write_buffer_.available_for_read()
    );
    
    return bytes_written;
}

void Session::try_parse_messages() {
    while (true) {
        size_t bytes_consumed = 0;
        auto parse_result = parser_.try_parse(read_buffer_.get_read_view(), bytes_consumed);
        
        if (!parse_result.has_value()) {
            // Нужно больше данных
            break;
        }
        
        // Сообщение распаршено успешно
        const JsonMessage& msg = *parse_result;
        
        if (msg.is_request()) {
            RequestMessage request;
            request.id = msg.id;
            request.method = msg.method.value_or("");
            request.params = msg.params.value_or(nlohmann::json::object());
            handle_message(request);
        } else if (msg.is_error_response()) {
            // Это ошибка парсинга от сервера, логируем
            logger_->warn("Session", "Received error response");
        }
        
        // Продвигаем позицию чтения на размер распаршенного сообщения
        read_buffer_.advance_read(bytes_consumed);
        read_buffer_.compact();
    }
}

void Session::handle_message(const RequestMessage& msg) {
    logger_->debug("Session", "Session {} received method: {}", session_id_, msg.method);
    
    if (message_handler_) {
        try {
            message_handler_(msg);
        } catch (const std::exception& e) {
            logger_->error("Session", "Exception in message handler: {}", e.what());
            
            if (error_handler_) {
                ErrorResponseMessage error_response;
                error_response.id = msg.id;
                error_response.error.code = ProtocolErrorCode::InternalError;
                error_response.error.message = std::string("Internal error: ") + e.what();
                error_handler_(error_response.error);
            }
        }
    }
}

} // namespace demo_daemon::ipc
