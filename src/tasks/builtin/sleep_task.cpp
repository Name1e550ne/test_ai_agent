#include "tasks/sleep_task.hpp"
#include "core/logger.hpp"

namespace demo_daemon::tasks {

SleepTask::SleepTask(std::chrono::milliseconds duration_ms)
    : duration_(duration_ms) {}

std::string SleepTask::name() const {
    return "sleep";
}

std::string SleepTask::description() const {
    return "One-shot task that sleeps for specified duration with early cancellation support";
}

TaskResult SleepTask::run(std::stop_token stop_token) {
    auto& logger = core::Logger::instance();
    
    logger.info("SleepTask started for {} ms", duration_.count());
    
    auto start = std::chrono::steady_clock::now();
    auto remaining = duration_;
    
    // Split sleep into small intervals to check stop_token frequently
    constexpr auto check_interval = std::chrono::milliseconds(100);
    
    while (remaining.count() > 0 && !stop_token.stop_requested()) {
        auto sleep_duration = std::min(remaining, check_interval);
        std::this_thread::sleep_for(sleep_duration);
        
        remaining = duration_ - (std::chrono::steady_clock::now() - start);
    }
    
    if (stop_token.stop_requested()) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        logger.info("SleepTask cancelled after {} ms (requested: {} ms)", 
                    std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(),
                    duration_.count());
        return TaskResult::stopped();
    }
    
    logger.info("SleepTask completed successfully after {} ms", duration_.count());
    return TaskResult::completed();
}

} // namespace demo_daemon::tasks
