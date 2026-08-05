#pragma once

#include "demo_daemon/tasks/itask.hpp"
#include <chrono>
#include <atomic>
#include <thread>
#include <stop_token>

namespace demo_daemon::tasks {

/**
 * @brief Periodic task that logs heartbeat messages at specified interval.
 * 
 * Demonstrates periodic task pattern with cooperative cancellation.
 */
class HeartbeatTask : public ITask {
public:
    /**
     * @brief Construct a new Heartbeat Task
     * @param interval_ms Interval between heartbeats in milliseconds
     */
    explicit HeartbeatTask(std::chrono::milliseconds interval_ms = std::chrono::seconds(5));

    ~HeartbeatTask() override = default;

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] std::string type() const override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] TaskState state() const override;
    [[nodiscard]] bool start(std::stop_token stop_token) override;
    void request_stop() override;
    [[nodiscard]] bool wait_stopped(std::chrono::milliseconds timeout) override;
    [[nodiscard]] std::string last_error() const override;

private:
    std::string id_;
    std::chrono::milliseconds interval_;
    std::atomic<TaskState> state_{TaskState::Pending};
    std::atomic<bool> stopped_{false};
    std::string last_error_;
    mutable std::mutex mutex_;
    std::thread worker_thread_;
    std::stop_token stored_stop_token_;
    
    void run_loop(std::stop_token token);
};

} // namespace demo_daemon::tasks
