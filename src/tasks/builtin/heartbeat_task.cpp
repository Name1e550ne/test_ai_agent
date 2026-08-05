#include "demo_daemon/tasks/heartbeat_task.hpp"
#include "demo_daemon/core/logger.hpp"

namespace demo_daemon::tasks {

namespace {
    std::string generate_id() {
        // Simple ID generation using random numbers
        static std::atomic<std::uint64_t> counter{0};
        auto id = counter.fetch_add(1, std::memory_order_relaxed);
        return "heartbeat_" + std::to_string(id);
    }
}

HeartbeatTask::HeartbeatTask(std::chrono::milliseconds interval_ms)
    : id_(generate_id())
    , interval_(interval_ms) {}

std::string HeartbeatTask::id() const {
    return id_;
}

std::string HeartbeatTask::type() const {
    return "heartbeat";
}

std::string HeartbeatTask::description() const {
    return "Periodic heartbeat task that logs status at specified interval";
}

TaskState HeartbeatTask::state() const {
    return state_.load(std::memory_order_acquire);
}

bool HeartbeatTask::start(std::stop_token stop_token) {
    if (state_.load(std::memory_order_acquire) != TaskState::Pending) {
        last_error_ = "Task already started";
        return false;
    }
    
    state_.store(TaskState::Running, std::memory_order_release);
    stored_stop_token_ = stop_token;
    
    try {
        worker_thread_ = std::thread(&HeartbeatTask::run_loop, this, stop_token);
        return true;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        state_.store(TaskState::Failed, std::memory_order_release);
        return false;
    }
}

void HeartbeatTask::request_stop() {
    state_.store(TaskState::Stopping, std::memory_order_release);
}

bool HeartbeatTask::wait_stopped(std::chrono::milliseconds timeout) {
    if (!worker_thread_.joinable()) {
        return true;
    }
    
    if (timeout.count() <= 0) {
        worker_thread_.join();
        return true;
    }
    
    // For simplicity, we just join with no timeout support in this implementation
    // A more sophisticated version would use condition_variable
    worker_thread_.join();
    return stopped_.load(std::memory_order_acquire);
}

std::string HeartbeatTask::last_error() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

void HeartbeatTask::run_loop(std::stop_token token) {
    Logger& logger = Logger::instance();
    
    logger.info("HeartbeatTask started with interval {} ms", interval_.count());
    
    int beat_count = 0;
    
    while (!token.stop_requested()) {
        ++beat_count;
        logger.info("Heartbeat #{} - service is alive", beat_count);
        
        // Wait for interval or stop signal
        std::this_thread::sleep_for(interval_);
    }
    
    logger.info("HeartbeatTask stopped after {} beats", beat_count);
    state_.store(TaskState::Stopped, std::memory_order_release);
    stopped_.store(true, std::memory_order_release);
}

} // namespace demo_daemon::tasks
