#pragma once

#include "../common/shared_array.hpp"
#include "../compat/native_access_fault.hpp"
#include "../compat/retained_string.hpp"
#include <functional>
#include <map>
#include <memory>
#include <type_traits>
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

// A Source delegate copy retains the same callable context, including mutable state.
class TaskHandler {
    using Function = std::function<void(RenderContext&)>;
    std::shared_ptr<Function> function_;
public:
    TaskHandler() = default;
    TaskHandler(std::nullptr_t) noexcept {}
    TaskHandler(const TaskHandler&) = default;
    TaskHandler& operator=(const TaskHandler&) = default;
    TaskHandler(TaskHandler&& other) noexcept : TaskHandler(static_cast<const TaskHandler&>(other)) {}
    TaskHandler& operator=(TaskHandler&& other) noexcept {
        return operator=(static_cast<const TaskHandler&>(other));
    }
    template<class F> requires (!std::is_same_v<std::remove_cvref_t<F>, TaskHandler> &&
        std::is_constructible_v<Function, F&&>)
    TaskHandler(F&& function) : function_(std::make_shared<Function>(std::forward<F>(function))) {}
    explicit operator bool() const noexcept { return function_ && static_cast<bool>(*function_); }
    void operator()(RenderContext& context) const {
        const auto function = function_;
        if (!function || !*function) d_runtime::ngFailNullAccess(false);
        (*function)(context);
    }
};

struct Task {
    TaskOrder order{TaskOrder::Init};
    TaskKind kind{TaskKind::Init};
    TaskHandler handler;
};

class TaskScheduler : public std::enable_shared_from_this<TaskScheduler> {
public:
    TaskScheduler();
    virtual ~TaskScheduler() = default;

    virtual void clearTasks();
    virtual void addTask(TaskOrder order, TaskKind kind, TaskHandler handler);
    virtual void executeRange(RenderContext& ctx, TaskOrder startOrder, TaskOrder endOrder = TaskOrder::Final);
    virtual void execute(RenderContext& ctx) {
        [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        executeRange(ctx, TaskOrder::Init, TaskOrder::Final);
    }
    std::size_t taskCount(TaskOrder order) const;
    std::size_t totalTaskCount() const;

    // Original inherited Object methods, appended after existing native slots.
    const void* objectIdentity() const noexcept { return dynamic_cast<const void*>(this); }
    virtual d_runtime::RetainedString toString();
    virtual std::size_t toHash() noexcept;
    virtual bool opEquals(std::shared_ptr<TaskScheduler> other);
    virtual int opCmp(std::shared_ptr<TaskScheduler> other);

private:
    std::map<TaskOrder, common::SharedArray<Task>> queues_;
    std::vector<TaskOrder> orderSequence_;
};

} // namespace nicxlive::core
