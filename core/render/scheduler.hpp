#pragma once

#include <functional>
#include <utility>
#include <vector>

namespace nicxlive::core {

class RenderContext;

enum class TaskOrder {
    Init,
    Parameters,
    PreProcess,
    Dynamic,
    Post0,
    Post1,
    Post2,
    RenderBegin,
    Render,
    Final,
    RenderEnd,
};

enum class TaskKind {
    Init,
    Parameters,
    PreProcess,
    Dynamic,
    PostProcess,
    Render,
    Finalize,
};

class TaskScheduler {
public:
    using TaskFn = std::function<void(RenderContext&)>;

    void clearTasks() { tasks_.clear(); }

    void addTask(TaskOrder order, TaskKind /*kind*/, TaskFn fn) {
        tasks_.push_back(std::make_pair(order, std::move(fn)));
    }

    void executeRange(RenderContext& ctx, TaskOrder begin, TaskOrder end) {
        for (auto& [order, fn] : tasks_) {
            if (static_cast<int>(order) < static_cast<int>(begin)) continue;
            if (static_cast<int>(order) > static_cast<int>(end)) continue;
            if (fn) fn(ctx);
        }
    }

private:
    std::vector<std::pair<TaskOrder, TaskFn>> tasks_{};
};

} // namespace nicxlive::core
