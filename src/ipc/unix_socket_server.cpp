#include "demo_daemon/ipc/unix_socket_server.hpp"

#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>

namespace demo_daemon::ipc {

UnixSocketServer::UnixSocketServer(UnixSocketConfig config,
                                   std::shared_ptr<Logger> logger)
    : config_(std::move(config))
    , logger_(std::move(logger))
{
}

UnixSocketServer::~UnixSocketServer() {
    stop();
    
    if (server_fd_ >= 0) {
        ::close(server_fd_);
        server_fd_ = -1;
    }
    
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
}

bool UnixSocketServer::initialize() {
    // Очищаем старый сокет если существует
    if (!cleanup_socket(config_.socket_path)) {
        logger_->warn("UnixSocketServer", 
                      "Failed to cleanup old socket file at {}", 
                      config_.socket_path);
    }
    
    // Создаем серверный сокет
    server_fd_ = create_server_socket();
    if (server_fd_ < 0) {
        logger_->error("UnixSocketServer", "Failed to create server socket");
        return false;
    }
    
    // Создаем epoll
    epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0) {
        logger_->error("UnixSocketServer", 
                       "Failed to create epoll: {}", strerror(errno));
        ::close(server_fd_);
        server_fd_ = -1;
        return false;
    }
    
    // Добавляем серверный сокет в epoll
    if (!add_to_epoll(server_fd_, EPOLLIN | EPOLLET, nullptr)) {
        logger_->error("UnixSocketServer", "Failed to add server socket to epoll");
        ::close(epoll_fd_);
        ::close(server_fd_);
        epoll_fd_ = -1;
        server_fd_ = -1;
        return false;
    }
    
    logger_->info("UnixSocketServer", 
                  "Server initialized on socket: {}", config_.socket_path);
    
    return true;
}

bool UnixSocketServer::run(CommandHandler command_handler) {
    if (server_fd_ < 0 || epoll_fd_ < 0) {
        logger_->error("UnixSocketServer", "Server not initialized");
        return false;
    }
    
    running_ = true;
    stop_requested_ = false;
    
    constexpr int MAX_EVENTS = 64;
    std::vector<struct epoll_event> events(MAX_EVENTS);
    
    logger_->info("UnixSocketServer", "Server started, waiting for connections...");
    
    while (!stop_requested_) {
        int nfds = ::epoll_wait(epoll_fd_, events.data(), static_cast<int>(events.size()), -1);
        
        if (nfds < 0) {
            if (errno == EINTR) {
                // Прервано сигналом, продолжаем
                continue;
            }
            
            logger_->error("UnixSocketServer", 
                           "epoll_wait error: {}", strerror(errno));
            break;
        }
        
        for (int i = 0; i < nfds; ++i) {
            handle_epoll_event(events[i], command_handler);
        }
    }
    
    running_ = false;
    logger_->info("UnixSocketServer", "Server stopped");
    
    return !stop_requested_;
}

void UnixSocketServer::stop() {
    stop_requested_ = true;
    
    // Закрываем все соединения
    connection_manager_.close_all();
}

size_t UnixSocketServer::active_connections() const {
    return connection_manager_.session_count();
}

bool UnixSocketServer::cleanup_socket(const std::string& socket_path) {
    struct stat st;
    if (::stat(socket_path.c_str(), &st) == 0) {
        if (S_ISSOCK(st.st_mode)) {
            if (::unlink(socket_path.c_str()) == 0) {
                return true;
            }
        }
    }
    
    // Файл не существует или не сокет - это OK
    return errno == ENOENT;
}

int UnixSocketServer::create_server_socket() {
    int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0) {
        logger_->error("UnixSocketServer", 
                       "Failed to create socket: {}", strerror(errno));
        return -1;
    }
    
    // Настраиваем адрес
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    
    // Проверяем длину пути
    if (config_.socket_path.size() >= sizeof(addr.sun_path)) {
        logger_->error("UnixSocketServer", 
                       "Socket path too long: {} chars (max {})", 
                       config_.socket_path.size(), sizeof(addr.sun_path) - 1);
        ::close(fd);
        return -1;
    }
    
    std::strncpy(addr.sun_path, config_.socket_path.c_str(), sizeof(addr.sun_path) - 1);
    
    // Bind
    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        logger_->error("UnixSocketServer", 
                       "Failed to bind socket: {}", strerror(errno));
        ::close(fd);
        return -1;
    }
    
    // Выставляем права на сокет
    if (::chmod(config_.socket_path.c_str(), 
                static_cast<mode_t>(config_.socket_permissions)) < 0) {
        logger_->warn("UnixSocketServer", 
                      "Failed to set socket permissions: {}", strerror(errno));
    }
    
    // Listen
    if (::listen(fd, config_.backlog) < 0) {
        logger_->error("UnixSocketServer", 
                       "Failed to listen: {}", strerror(errno));
        ::close(fd);
        return -1;
    }
    
    logger_->debug("UnixSocketServer", 
                   "Server socket created (fd={}, path={}, permissions={:o})",
                   fd, config_.socket_path, config_.socket_permissions);
    
    return fd;
}

bool UnixSocketServer::add_to_epoll(int fd, uint32_t events, void* data) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = data;
    
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        logger_->error("UnixSocketServer", 
                       "epoll_ctl ADD failed: {}", strerror(errno));
        return false;
    }
    
    return true;
}

bool UnixSocketServer::modify_epoll(int fd, uint32_t events, void* data) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = data;
    
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
        logger_->error("UnixSocketServer", 
                       "epoll_ctl MOD failed: {}", strerror(errno));
        return false;
    }
    
    return true;
}

bool UnixSocketServer::remove_from_epoll(int fd) {
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        logger_->warn("UnixSocketServer", 
                      "epoll_ctl DEL failed: {}", strerror(errno));
        return false;
    }
    
    return true;
}

int UnixSocketServer::accept_connection() {
    struct sockaddr_un client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = ::accept4(
        server_fd_,
        reinterpret_cast<struct sockaddr*>(&client_addr),
        &client_len,
        SOCK_CLOEXEC | SOCK_NONBLOCK
    );
    
    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -1; // Нет pending соединений
        }
        
        logger_->warn("UnixSocketServer", 
                      "accept failed: {}", strerror(errno));
        return -1;
    }
    
    // Проверяем лимит подключений
    if (connection_manager_.session_count() >= config_.max_connections) {
        logger_->warn("UnixSocketServer", 
                      "Max connections reached ({})", config_.max_connections);
        ::close(client_fd);
        return -1;
    }
    
    return client_fd;
}

void UnixSocketServer::handle_epoll_event(const struct epoll_event& event,
                                          CommandHandler command_handler) {
    // Событие на серверном сокете - новое соединение
    if (event.data.ptr == nullptr) {
        while (true) {
            int client_fd = accept_connection();
            if (client_fd < 0) {
                break;
            }
            
            logger_->debug("UnixSocketServer", 
                           "New connection accepted (fd={})", client_fd);
            
            // Создаем сессию
            auto session = std::make_shared<Session>(
                client_fd,
                logger_,
                [this, &command_handler](const RequestMessage& msg) {
                    auto session_ptr = connection_manager_.get_session_by_fd(
                        static_cast<int>(msg.id.value_or(0))
                    );
                    if (session_ptr) {
                        on_message(msg, session_ptr, command_handler);
                    }
                },
                [this](const ProtocolError& error) {
                    // Обработка ошибок парсинга
                    logger_->warn("UnixSocketServer", 
                                  "Protocol error: code={}, message={}", 
                                  static_cast<int>(error.code), error.message);
                }
            );
            
            if (!session->initialize()) {
                logger_->error("UnixSocketServer", 
                               "Failed to initialize session (fd={})", client_fd);
                ::close(client_fd);
                continue;
            }
            
            // Регистрируем сессию и добавляем в epoll
            connection_manager_.add_session(session);
            
            struct epoll_event ev;
            ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
            ev.data.ptr = reinterpret_cast<void*>(static_cast<uintptr_t>(client_fd));
            
            if (!add_to_epoll(client_fd, ev.events, ev.data.ptr)) {
                logger_->error("UnixSocketServer", 
                               "Failed to add client to epoll (fd={})", client_fd);
                connection_manager_.remove_session_by_fd(client_fd);
                ::close(client_fd);
            }
        }
        
        return;
    }
    
    // Событие на клиентском сокете
    int client_fd = static_cast<int>(
        reinterpret_cast<uintptr_t>(event.data.ptr)
    );
    
    auto session = connection_manager_.get_session_by_fd(client_fd);
    if (!session) {
        // Сессия уже удалена, убираем из epoll
        remove_from_epoll(client_fd);
        return;
    }
    
    bool session_active = true;
    
    if (event.events & (EPOLLIN | EPOLLERR | EPOLLHUP)) {
        session_active = session->on_readable();
    }
    
    if (session_active && (event.events & EPOLLOUT)) {
        session_active = session->on_writable();
    }
    
    if (!session_active || session->is_closed()) {
        logger_->debug("UnixSocketServer", 
                       "Closing session (fd={})", client_fd);
        remove_from_epoll(client_fd);
        connection_manager_.remove_session_by_fd(client_fd);
        // FD будет закрыт в деструкторе сессии
    }
}

void UnixSocketServer::on_message(const RequestMessage& msg,
                                  std::shared_ptr<Session> session,
                                  CommandHandler command_handler) {
    try {
        ResponseMessage response = command_handler(msg);
        
        // Устанавливаем тот же ID что и в запросе
        response.id = msg.id;
        
        session->schedule_send(response);
        
        // Обновляем epoll чтобы включить EPOLLOUT если нужно
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
        ev.data.ptr = reinterpret_cast<void*>(
            static_cast<uintptr_t>(session->get_fd())
        );
        modify_epoll(session->get_fd(), ev.events, ev.data.ptr);
        
    } catch (const std::exception& e) {
        logger_->error("UnixSocketServer", 
                       "Command handler exception: {}", e.what());
        
        ErrorResponseMessage error_response;
        error_response.id = msg.id;
        error_response.error.code = ProtocolErrorCode::InternalError;
        error_response.error.message = std::string("Internal error: ") + e.what();
        
        session->schedule_send_error(error_response);
    }
}

} // namespace demo_daemon::ipc
