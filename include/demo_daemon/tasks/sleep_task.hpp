#pragma once

#include "demo_daemon/tasks/itask.hpp"
#include <chrono>
#include <atomic>
#include <thread>
#include <stop_token>

namespace demo_daemon::tasks {

/**
 * @brief One-shot task that sleeps for specified duration.
 * 
 * Demonstrates one-shot task pattern with early cancellation support.
 * Can be stopped before the sleep duration completes.
 */
class SleepTask : public ITask {
public:
    /**
     * @brief Construct a new Sleep Task
     * @param duration_ms Duration to sleep in milliseconds
     */
    explicit SleepTask(std::chrono::milliseconds duration_ms);

    ~SleepTask() override = default;

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
    std::chrono::milliseconds duration_;
    std::atomic<TaskState> state_{TaskState::Pending};
    std::atomic<bool> stopped_{false};
    std::string last_error_;
    mutable std::mutex mutex_;
    std::thread worker_thread_;
    
    void run_loop(std::stop_token token);
};

} // namespace demo_daemon::tasks
