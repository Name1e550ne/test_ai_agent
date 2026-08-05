#include "demo_daemon/tasks/task_factory.hpp"
#include "demo_daemon/tasks/heartbeat_task.hpp"
#include "demo_daemon/tasks/sleep_task.hpp"

namespace demo_daemon::tasks {

bool TaskFactory::registerTask(const std::string& type_name, CreatorFunc creator) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (creators_.count(type_name) > 0) {
        return false; // Already registered
    }
    
    creators_[type_name] = std::move(creator);
    return true;
}

std::unique_ptr<ITask> TaskFactory::createTask(
    const std::string& type_name, 
    const nlohmann::json& params) const {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = creators_.find(type_name);
    if (it == creators_.end()) {
        return nullptr;
    }
    
    return it->second(params);
}

std::vector<std::string> TaskFactory::getRegisteredTypes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> types;
    types.reserve(creators_.size());
    
    for (const auto& [name, _] : creators_) {
        types.push_back(name);
    }
    
    return types;
}

TaskFactory& TaskFactory::instance() {
    static TaskFactory instance;
    return instance;
}

// Auto-register built-in tasks at startup
namespace {
    struct BuiltinTaskRegistrar {
        BuiltinTaskRegistrar() {
            auto& factory = TaskFactory::instance();
            
            // Register heartbeat task
            factory.registerTask("heartbeat", [](const nlohmann::json& params) {
                int interval_sec = 5;
                if (params.contains("interval")) {
                    interval_sec = params["interval"].get<int>();
                }
                return std::make_unique<HeartbeatTask>(
                    std::chrono::seconds(interval_sec));
            });
            
            // Register sleep task
            factory.registerTask("sleep", [](const nlohmann::json& params) {
                int duration_sec = 10;
                if (params.contains("duration")) {
                    duration_sec = params["duration"].get<int>();
                }
                return std::make_unique<SleepTask>(
                    std::chrono::seconds(duration_sec));
            });
        }
    };
    
    [[maybe_unused]] BuiltinTaskRegistrar registrar;
}

} // namespace demo_daemon::tasks
