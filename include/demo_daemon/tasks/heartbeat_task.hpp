#pragma once

#include "tasks/itask.hpp"
#include <chrono>

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

    std::string name() const override;
    std::string description() const override;
    
    [[nodiscard]] TaskResult run(std::stop_token stop_token) override;

private:
    std::chrono::milliseconds interval_;
};

} // namespace demo_daemon::tasks
