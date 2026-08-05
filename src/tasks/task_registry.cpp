#include "demo_daemon/tasks/task_registry.hpp"
#include <mutex>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace demo_daemon::tasks {

bool TaskRegistry::register_task_type(const std::string& type, TaskFactory factory) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (factories_.count(type) > 0) {
        return false; // Тип уже зарегистрирован
    }
    
    factories_[type] = std::move(factory);
    return true;
}

std::unique_ptr<ITask> TaskRegistry::create_task(
    const std::string& type,
    const std::string& id,
    const nlohmann::json& params) 
{
    TaskFactory factory;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = factories_.find(type);
        if (it == factories_.end()) {
            return nullptr;
        }
        factory = it->second;
    }
    
    return factory(id, params);
}

std::vector<std::string> TaskRegistry::get_registered_types() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> types;
    types.reserve(factories_.size());
    
    for (const auto& [type, _] : factories_) {
        types.push_back(type);
    }
    
    std::sort(types.begin(), types.end());
    return types;
}

bool TaskRegistry::has_type(const std::string& type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return factories_.count(type) > 0;
}

} // namespace demo_daemon::tasks
