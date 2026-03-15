#include "scheduler.hpp"
#include "profiler.hpp"
#include "graph_builder.hpp"
#include "common.hpp"
#include <string>

namespace nicxlive::core {

namespace {
const char* taskOrderLabel(TaskOrder order) {
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
    default: return "UnknownOrder";
    }
}

const char* taskKindLabel(TaskKind kind) {
    switch (kind) {
    case TaskKind::Init: return "Init";
    case TaskKind::Parameters: return "Parameters";
    case TaskKind::PreProcess: return "PreProcess";
    case TaskKind::Dynamic: return "Dynamic";
    case TaskKind::PostProcess: return "PostProcess";
    case TaskKind::Render: return "Render";
    case TaskKind::Finalize: return "Finalize";
    default: return "UnknownKind";
    }
}
} // namespace

TaskScheduler::TaskScheduler() {
    orderSequence_ = {TaskOrder::Init, TaskOrder::Parameters, TaskOrder::PreProcess, TaskOrder::Dynamic,
                      TaskOrder::Post0, TaskOrder::Post1, TaskOrder::Post2, TaskOrder::Final,
                      TaskOrder::RenderBegin, TaskOrder::Render, TaskOrder::RenderEnd};
    for (auto order : orderSequence_) queues_[order] = {};
}

void TaskScheduler::clearTasks() {
    for (auto order : orderSequence_) {
        queues_[order].clear();
    }
}

void TaskScheduler::addTask(TaskOrder order, TaskKind kind, TaskHandler handler) {
    queues_[order].push_back(Task{order, kind, std::move(handler)});
}

void TaskScheduler::executeRange(RenderContext& ctx, TaskOrder startOrder, TaskOrder endOrder) {
    for (auto order : orderSequence_) {
        if (static_cast<int>(order) < static_cast<int>(startOrder)) continue;
        if (static_cast<int>(order) > static_cast<int>(endOrder)) break;
        auto& tasks = queues_[order];
        for (auto& task : tasks) {
            if (task.handler) {
                auto label = std::string("Task.") + taskOrderLabel(order) + "." + taskKindLabel(task.kind);
                auto profiling = render::profileScope(label.c_str());
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
