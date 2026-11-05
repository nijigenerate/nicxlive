#include "scheduler.hpp"
#include "profiler.hpp"
#include "graph_builder.hpp"
#include "common.hpp"
#include <string>
#include <mutex>
#include "../compat/object_metadata.hpp"
#include "../compat/throwable.hpp"

namespace nicxlive::core {

namespace {
void registerTaskSchedulerClassName() {
    static std::once_flag initialized;
    std::call_once(initialized, [] {
        d_runtime::ngRegisterObjectClassName(typeid(TaskScheduler),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.render.scheduler.TaskScheduler"));
    });
}
std::string taskOrderLabel(TaskOrder order) {
    switch (order) {
    case TaskOrder::Init: return "Init";
    case TaskOrder::Parameters: return "Parameters";
    case TaskOrder::PreProcess: return "PreProcess";
    case TaskOrder::Dynamic: return "Dynamic";
    case TaskOrder::Post0: return "Post0";
    case TaskOrder::Post1: return "Post1";
    case TaskOrder::Post2: return "Post2";
    case TaskOrder::RenderBegin: return "RenderBegin";
    case TaskOrder::Render: return "Render";
    case TaskOrder::RenderEnd: return "RenderEnd";
    case TaskOrder::Final: return "Final";
    default: return "cast(TaskOrder)" + std::to_string(static_cast<int>(order));
    }
}

std::string taskKindLabel(TaskKind kind) {
    switch (kind) {
    case TaskKind::Init: return "Init";
    case TaskKind::Parameters: return "Parameters";
    case TaskKind::PreProcess: return "PreProcess";
    case TaskKind::Dynamic: return "Dynamic";
    case TaskKind::PostProcess: return "PostProcess";
    case TaskKind::Render: return "Render";
    case TaskKind::Finalize: return "Finalize";
    default: return "cast(TaskKind)" + std::to_string(static_cast<int>(kind));
    }
}
} // namespace

// Object protocol adapted from object.d, Digital Mars 2000-2011, BSL-1.0.
// See core/licenses/druntime-associative-array.txt.
d_runtime::RetainedString TaskScheduler::toString() {
    registerTaskSchedulerClassName();
    return d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t TaskScheduler::toHash() noexcept {
    const auto address = reinterpret_cast<std::size_t>(objectIdentity());
    return address ^ (address >> 4);
}
bool TaskScheduler::opEquals(std::shared_ptr<TaskScheduler> other) {
    return other && objectIdentity() == other->objectIdentity();
}
int TaskScheduler::opCmp(std::shared_ptr<TaskScheduler>) {
    registerTaskSchedulerClassName();
    std::string message = "need opCmp for class ";
    message += d_runtime::ngObjectClassName(typeid(*this)).view();
    throw d_runtime::ThrowableException(d_runtime::RetainedString(std::move(message)),
        d_runtime::RetainedString::fromStaticLiteral("object.d"), 195);
}

TaskScheduler::TaskScheduler() {
    orderSequence_ = {TaskOrder::Init, TaskOrder::Parameters, TaskOrder::PreProcess, TaskOrder::Dynamic,
                      TaskOrder::Post0, TaskOrder::Post1, TaskOrder::Post2, TaskOrder::Final,
                      TaskOrder::RenderBegin, TaskOrder::Render, TaskOrder::RenderEnd};
    for (auto order : orderSequence_) queues_[order] = {};
}

void TaskScheduler::clearTasks() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    for (auto order : orderSequence_) {
        queues_[order].resize(0);
    }
}

void TaskScheduler::addTask(TaskOrder order, TaskKind kind, TaskHandler handler) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    queues_[order].push_back(Task{order, kind, std::move(handler)});
}

void TaskScheduler::executeRange(RenderContext& ctx, TaskOrder startOrder, TaskOrder endOrder) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    for (auto order : orderSequence_) {
        if (static_cast<int>(order) < static_cast<int>(startOrder)) continue;
        if (static_cast<int>(order) > static_cast<int>(endOrder)) break;
        const auto tasks = queues_[order];
        for (auto task : tasks) {
            if (task.handler) {
                auto label = std::string("Task.") + taskOrderLabel(order) + "." + taskKindLabel(task.kind);
                auto profiling = render::profileScope(label);
                task.handler(ctx);
            }
        }
    }
}

std::size_t TaskScheduler::taskCount(TaskOrder order) const {
    auto it = queues_.find(order);
    if (it == queues_.end()) return 0;
    return it->second.size();
}

std::size_t TaskScheduler::totalTaskCount() const {
    std::size_t total = 0;
    for (const auto& kv : queues_) total += kv.second.size();
    return total;
}

} // namespace nicxlive::core
