#include "demo_daemon/tasks/task_manager.hpp"
#include "demo_daemon/core/logger.hpp"
#include <sstream>

namespace demo_daemon::tasks {

TaskManager::TaskManager(std::shared_ptr<TaskRegistry> registry)
    : registry_(std::move(registry)) 
{
    if (!registry_) {
        throw std::invalid_argument("TaskRegistry cannot be null");
    }
    
    LOG_INFO_FMT("TaskManager initialized");
}

TaskManager::~TaskManager() {
    // Останавливаем все задачи перед уничтожением
    stop_all_tasks();
    LOG_INFO_FMT("TaskManager destroyed");
}

std::string TaskManager::add_task(const std::string& type, const nlohmann::json& params) {
    if (shutdown_requested_.load(std::memory_order_acquire)) {
        LOG_WARN_FMT("Cannot add task: shutdown requested");
        return "";
    }

    // Создаем задачу через registry
    std::stringstream ss;
    ss << "task_" << next_task_id_.fetch_add(1, std::memory_order_relaxed);
    std::string task_id = ss.str();

    auto task = registry_->create_task(type, task_id, params);
    if (!task) {
        LOG_ERROR_FMT("Failed to create task of type '{}': type not registered", type);
        return "";
    }

    LOG_INFO_FMT("Creating task '{}' of type '{}'", task_id, type);

    TaskEntry entry;
    entry.task = std::move(task);
    entry.created_at = std::chrono::steady_clock::now();
    entry.started_at = std::chrono::steady_clock::time_point::min();
    entry.stopped_at = std::chrono::steady_clock::time_point::min();

    // Вставляем задачу в мапу, используя piecewise construction для обхода проблемы с std::atomic
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        TaskEntry entry;
        entry.task = std::move(task);
        entry.created_at = std::chrono::steady_clock::now();
        entry.started_at = std::chrono::steady_clock::time_point::min();
        entry.stopped_at = std::chrono::steady_clock::time_point::min();
        entry.stop_source = std::stop_source();
        
        auto result = tasks_.try_emplace(task_id, std::move(entry));
        if (!result.second) {
            LOG_ERROR_FMT("Task '{}' already exists", task_id);
            return "";
        }
        
        TaskEntry& ref = result.first->second;
        
        // Запускаем поток
        ref.thread = std::thread([this, &ref]() {
            auto token = ref.stop_source.get_token();
            run_task(ref, token);
        });
        
        // Отделяем поток (мы сами управляем его временем жизни)
        ref.thread.detach();
    }

    LOG_INFO_FMT("Task '{}' added successfully", task_id);
    return task_id;
}

void TaskManager::run_task(TaskEntry& entry, std::stop_token token) {
    entry.started_at = std::chrono::steady_clock::now();
    entry.running.store(true, std::memory_order_release);
    
    try {
        LOG_INFO_FMT("Starting task '{}'", entry.task->id());
        
        if (!entry.task->start(token)) {
            LOG_ERROR_FMT("Task '{}' failed to start", entry.task->id());
            entry.task->last_error(); // Получаем ошибку если есть
        }
        
        // Ждем завершения задачи
        entry.task->wait_stopped(std::chrono::milliseconds(0)); // 0 = ждем бесконечно
        
        entry.stopped_at = std::chrono::steady_clock::now();
        entry.running.store(false, std::memory_order_release);
        LOG_INFO_FMT("Task '{}' finished", entry.task->id());
        
    } catch (const std::exception& e) {
        entry.stopped_at = std::chrono::steady_clock::now();
        entry.running.store(false, std::memory_order_release);
        LOG_ERROR_FMT("Task '{}' threw exception: {}", entry.task->id(), e.what());
    } catch (...) {
        entry.stopped_at = std::chrono::steady_clock::now();
        entry.running.store(false, std::memory_order_release);
        LOG_ERROR_FMT("Task '{}' threw unknown exception", entry.task->id());
    }
}

bool TaskManager::stop_task(const std::string& task_id) {
    TaskEntry* entry = nullptr;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tasks_.find(task_id);
        if (it == tasks_.end()) {
            LOG_WARN_FMT("Task '{}' not found for stopping", task_id);
            return false;
        }
        entry = &it->second;
    }
    
    // Запрашиваем остановку вне блокировки
    LOG_INFO_FMT("Requesting stop for task '{}'", task_id);
    entry->stop_source.request_stop();
    entry->task->request_stop();
    
    return true;
}

void TaskManager::stop_all_tasks() {
    LOG_INFO_FMT("Stopping all tasks...");
    shutdown_requested_.store(true, std::memory_order_release);
    
    std::vector<std::string> task_ids;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [id, _] : tasks_) {
            task_ids.push_back(id);
        }
    }
    
    // Запрашиваем остановку всем задачам (игнорируем результат, т.к. задача может уже быть остановлена)
    for (const auto& id : task_ids) {
        (void)stop_task(id);
    }
    
    // Ждем завершения всех потоков (с таймаутом, так как потоки detached)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        
        for (auto& [id, entry] : tasks_) {
            if (entry.running.load(std::memory_order_acquire)) {
                LOG_DEBUG_FMT("Waiting for task '{}' to finish", id);
                
                // Ждем с polling (так как поток detached)
                while (entry.running.load(std::memory_order_acquire)) {
                    if (std::chrono::steady_clock::now() >= deadline) {
                        LOG_WARN_FMT("Timeout waiting for task '{}' to finish", id);
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            }
        }
    }
    
    LOG_INFO_FMT("All tasks stopped");
}

std::optional<TaskInfo> TaskManager::get_task_info(const std::string& task_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        return std::nullopt;
    }
    
    const TaskEntry& entry = it->second;
    
    TaskInfo info;
    info.id = entry.task->id();
    info.type = entry.task->type();
    info.description = entry.task->description();
    info.state = entry.task->state();
    info.last_error = entry.task->last_error();
    info.created_at = entry.created_at;
    info.started_at = entry.started_at;
    info.stopped_at = entry.stopped_at;
    
    return info;
}

std::vector<TaskInfo> TaskManager::list_tasks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<TaskInfo> result;
    result.reserve(tasks_.size());
    
    for (const auto& [id, entry] : tasks_) {
        TaskInfo info;
        info.id = entry.task->id();
        info.type = entry.task->type();
        info.description = entry.task->description();
        info.state = entry.task->state();
        info.last_error = entry.task->last_error();
        info.created_at = entry.created_at;
        info.started_at = entry.started_at;
        info.stopped_at = entry.stopped_at;
        
        result.push_back(std::move(info));
    }
    
    // Сортируем по ID для стабильного порядка
    std::sort(result.begin(), result.end(), [](const TaskInfo& a, const TaskInfo& b) {
        return a.id < b.id;
    });
    
    return result;
}

std::size_t TaskManager::active_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::size_t count = 0;
    for (const auto& [id, entry] : tasks_) {
        TaskState state = entry.task->state();
        if (state == TaskState::Running || state == TaskState::Stopping) {
            ++count;
        }
    }
    
    return count;
}

std::size_t TaskManager::total_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}

} // namespace demo_daemon::tasks
