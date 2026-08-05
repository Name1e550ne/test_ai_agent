#include "tasks/heartbeat_task.hpp"
#include "core/logger.hpp"

namespace demo_daemon::tasks {

HeartbeatTask::HeartbeatTask(std::chrono::milliseconds interval_ms)
    : interval_(interval_ms) {}

std::string HeartbeatTask::name() const {
    return "heartbeat";
}

std::string HeartbeatTask::description() const {
    return "Periodic heartbeat task that logs status at specified interval";
}

TaskResult HeartbeatTask::run(std::stop_token stop_token) {
    auto& logger = core::Logger::instance();
    
    logger.info("HeartbeatTask started with interval {} ms", interval_.count());
    
    int beat_count = 0;
    
    while (!stop_token.stop_requested()) {
        ++beat_count;
        logger.info("Heartbeat #{} - service is alive", beat_count);
        
        // Wait for interval or stop signal
        std::this_thread::sleep_for(interval_);
    }
    
    logger.info("HeartbeatTask stopped after {} beats", beat_count);
    return TaskResult::stopped();
}

} // namespace demo_daemon::tasks
