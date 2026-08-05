#include "demo_daemon/ipc/connection_manager.hpp"

namespace demo_daemon::ipc {

void ConnectionManager::add_session(SessionPtr session) {
    if (!session) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint64_t session_id = session->get_id();
    int fd = session->get_fd();
    
    sessions_by_id_[session_id] = std::move(session);
    fd_to_id_[fd] = session_id;
}

void ConnectionManager::remove_session(uint64_t session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_by_id_.find(session_id);
    if (it != sessions_by_id_.end()) {
        int fd = it->second->get_fd();
        fd_to_id_.erase(fd);
        sessions_by_id_.erase(it);
    }
}

void ConnectionManager::remove_session_by_fd(int fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = fd_to_id_.find(fd);
    if (it != fd_to_id_.end()) {
        uint64_t session_id = it->second;
        fd_to_id_.erase(it);
        sessions_by_id_.erase(session_id);
    }
}

ConnectionManager::SessionPtr ConnectionManager::get_session(uint64_t session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_by_id_.find(session_id);
    if (it != sessions_by_id_.end()) {
        return it->second;
    }
    return nullptr;
}

ConnectionManager::SessionPtr ConnectionManager::get_session_by_fd(int fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = fd_to_id_.find(fd);
    if (it != fd_to_id_.end()) {
        uint64_t session_id = it->second;
        auto session_it = sessions_by_id_.find(session_id);
        if (session_it != sessions_by_id_.end()) {
            return session_it->second;
        }
    }
    return nullptr;
}

size_t ConnectionManager::session_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_by_id_.size();
}

std::vector<ConnectionManager::SessionPtr> ConnectionManager::get_all_sessions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<SessionPtr> sessions;
    sessions.reserve(sessions_by_id_.size());
    
    for (const auto& [id, session] : sessions_by_id_) {
        sessions.push_back(session);
    }
    
    return sessions;
}

void ConnectionManager::close_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [id, session] : sessions_by_id_) {
        session->close();
    }
    
    sessions_by_id_.clear();
    fd_to_id_.clear();
}

} // namespace demo_daemon::ipc
