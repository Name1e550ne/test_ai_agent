#pragma once

#include "demo_daemon/ipc/session_manager.hpp"

#include <sys/epoll.h>
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <vector>

namespace demo_daemon::ipc {

/**
 * @brief Менеджер соединений - управляет всеми активными сессиями
 * 
 * Потокобезопасный класс для регистрации, хранения и удаления сессий.
 */
class ConnectionManager {
public:
    using SessionPtr = std::shared_ptr<Session>;
    
    ConnectionManager() = default;
    ~ConnectionManager() = default;
    
    // Запрещаем копирование
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;
    
    /**
     * @brief Зарегистрировать новую сессию
     * @param session сессия для регистрации
     */
    void add_session(SessionPtr session);
    
    /**
     * @brief Удалить сессию по ID
     * @param session_id ID сессии
     */
    void remove_session(uint64_t session_id);
    
    /**
     * @brief Удалить сессию по FD
     * @param fd файловый дескриптор
     */
    void remove_session_by_fd(int fd);
    
    /**
     * @brief Получить сессию по ID
     * @param session_id ID сессии
     * @return shared_ptr на сессию или nullptr если не найдена
     */
    [[nodiscard]] SessionPtr get_session(uint64_t session_id);
    
    /**
     * @brief Получить сессию по FD
     * @param fd файловый дескриптор
     * @return shared_ptr на сессию или nullptr если не найдена
     */
    [[nodiscard]] SessionPtr get_session_by_fd(int fd);
    
    /**
     * @brief Получить количество активных сессий
     */
    [[nodiscard]] size_t session_count() const;
    
    /**
     * @brief Получить все активные сессии
     */
    [[nodiscard]] std::vector<SessionPtr> get_all_sessions() const;
    
    /**
     * @brief Закрыть все сессии
     */
    void close_all();
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, SessionPtr> sessions_by_id_;
    std::unordered_map<int, uint64_t> fd_to_id_;
};

} // namespace demo_daemon::ipc
