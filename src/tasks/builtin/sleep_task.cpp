#include "demo_daemon/tasks/sleep_task.hpp"
#include "demo_daemon/core/logger.hpp"

namespace demo_daemon::tasks {

namespace {
    std::string generate_id() {
        static std::atomic<std::uint64_t> counter{0};
        auto id = counter.fetch_add(1, std::memory_order_relaxed);
        return "sleep_" + std::to_string(id);
    }
}

SleepTask::SleepTask(std::chrono::milliseconds duration_ms)
    : id_(generate_id())
    , duration_(duration_ms) {}

std::string SleepTask::id() const {
    return id_;
}

std::string SleepTask::type() const {
    return "sleep";
}

std::string SleepTask::description() const {
    return "One-shot task that sleeps for specified duration with early cancellation support";
}

TaskState SleepTask::state() const {
    return state_.load(std::memory_order_acquire);
}

bool SleepTask::start(std::stop_token stop_token) {
    if (state_.load(std::memory_order_acquire) != TaskState::Pending) {
        last_error_ = "Task already started";
        return false;
    }
    
    state_.store(TaskState::Running, std::memory_order_release);
    
    try {
        worker_thread_ = std::thread(&SleepTask::run_loop, this, stop_token);
        return true;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        state_.store(TaskState::Failed, std::memory_order_release);
        return false;
    }
}

void SleepTask::request_stop() {
    state_.store(TaskState::Stopping, std::memory_order_release);
}

bool SleepTask::wait_stopped(std::chrono::milliseconds timeout) {
    if (!worker_thread_.joinable()) {
        return true;
    }
    
    if (timeout.count() <= 0) {
        worker_thread_.join();
        return true;
    }
    
    worker_thread_.join();
    return stopped_.load(std::memory_order_acquire);
}

std::string SleepTask::last_error() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

void SleepTask::run_loop(std::stop_token token) {
    auto& logger = demo_daemon::core::Logger::instance();
    
    logger.info("SleepTask started for {} ms", duration_.count());
    
    auto start = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::milliseconds(0);
    
    constexpr auto check_interval = std::chrono::milliseconds(100);
    
    while (elapsed < duration_ && !token.stop_requested()) {
        auto sleep_duration = std::min(duration_ - elapsed, check_interval);
        std::this_thread::sleep_for(sleep_duration);
        
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
    }
    
    if (token.stop_requested()) {
        logger.info("SleepTask cancelled after {} ms (requested: {} ms)", 
                    elapsed.count(),
                    duration_.count());
        state_.store(TaskState::Stopped, std::memory_order_release);
        stopped_.store(true, std::memory_order_release);
        return;
    }
    
    logger.info("SleepTask completed successfully after {} ms", duration_.count());
    state_.store(TaskState::Stopped, std::memory_order_release);
    stopped_.store(true, std::memory_order_release);
}

} // namespace demo_daemon::tasks
