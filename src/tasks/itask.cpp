#include "demo_daemon/tasks/itask.hpp"

namespace demo_daemon::tasks {

std::string task_state_to_string(TaskState state) {
    switch (state) {
        case TaskState::Pending:
            return "pending";
        case TaskState::Running:
            return "running";
        case TaskState::Stopping:
            return "stopping";
        case TaskState::Stopped:
            return "stopped";
        case TaskState::Failed:
            return "failed";
        default:
            return "unknown";
    }
}

} // namespace demo_daemon::tasks
