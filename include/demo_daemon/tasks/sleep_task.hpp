#pragma once

#include "tasks/itask.hpp"
#include <chrono>

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

    std::string name() const override;
    std::string description() const override;
    
    [[nodiscard]] TaskResult run(std::stop_token stop_token) override;

private:
    std::chrono::milliseconds duration_;
};

} // namespace demo_daemon::tasks
