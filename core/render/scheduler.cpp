#include "scheduler.hpp"
#include "profiler.hpp"
#include "graph_builder.hpp"
#include "common.hpp"
#include <string>

namespace nicxlive::core {

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
                auto label = "Task." + std::to_string(static_cast<int>(order)) + "." + std::to_string(static_cast<int>(task.kind));
                auto profiling = render::profileScope(label.c_str());
                task.handler(ctx);
            }
        }
    }
}

} // namespace nicxlive::core
