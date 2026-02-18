#pragma once

#include <functional>
#include <map>
#include <utility>
#include <vector>

namespace nicxlive::core {

class RenderContext;

enum class TaskOrder : int {
    Init = 1,
    Parameters = 2,
    PreProcess = 3,
    Dynamic = 4,
    Post0 = 5,
    Post1 = 6,
    Post2 = 7,
    RenderBegin = 8,
    Render = 9,
    RenderEnd = 10,
    Final = 11,
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

using TaskHandler = std::function<void(RenderContext&)>;

struct Task {
    TaskOrder order;
    TaskKind kind;
    TaskHandler handler;
};

class TaskScheduler {
public:
    TaskScheduler();

    void clearTasks();
    void addTask(TaskOrder order, TaskKind kind, TaskHandler handler);
    void executeRange(RenderContext& ctx, TaskOrder startOrder, TaskOrder endOrder = TaskOrder::Final);
    void execute(RenderContext& ctx) { executeRange(ctx, TaskOrder::Init, TaskOrder::Final); }
    std::size_t taskCount(TaskOrder order) const;
    std::size_t totalTaskCount() const;

private:
    std::map<TaskOrder, std::vector<Task>> queues_;
    std::vector<TaskOrder> orderSequence_;
};

} // namespace nicxlive::core
