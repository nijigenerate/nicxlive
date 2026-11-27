#pragma once

#include "node.hpp"
#include "../param/parameter.hpp"
#include "../common/shared_array.hpp"

#include <functional>
#include <cstddef>
#include <memory>
#include <utility>
#include <type_traits>

namespace nicxlive::core::nodes {

using ::nicxlive::core::param::Parameter;

class NodeFilter {
public:
    virtual ~NodeFilter() = default;
    virtual void captureTarget(const std::shared_ptr<Node>& target) = 0;
    virtual void releaseTarget(const std::shared_ptr<Node>& target) = 0;
    virtual void dispose() = 0;
    virtual void applyDeformToChildren(common::SharedArray<std::shared_ptr<Parameter>> params, bool recursive = true) = 0;

    class SourceUpdate {
        using Function = std::function<void(Vec2Array)>;
        std::shared_ptr<Function> function_;
    public:
        SourceUpdate() noexcept = default;
        SourceUpdate(std::nullptr_t) noexcept {}
        SourceUpdate(const SourceUpdate&) = default;
        SourceUpdate& operator=(const SourceUpdate&) = default;
        SourceUpdate(SourceUpdate&& other) noexcept : SourceUpdate(static_cast<const SourceUpdate&>(other)) {}
        SourceUpdate& operator=(SourceUpdate&& other) noexcept {
            return operator=(static_cast<const SourceUpdate&>(other));
        }
        template<class F> requires (!std::is_same_v<std::remove_cvref_t<F>, SourceUpdate> &&
            std::is_constructible_v<Function, F&&>)
        SourceUpdate(F&& callable) {
            Function function(std::forward<F>(callable));
            if (function) function_ = std::make_shared<Function>(std::move(function));
        }
        explicit operator bool() const noexcept { return function_ && static_cast<bool>(*function_); }
        void operator()(Vec2Array value) const {
            const auto function = function_;
            if (!function || !*function) d_runtime::ngFailNullAccess(false);
            (*function)(value);
        }
    };
    class SourceTransferCondition {
        using Function = std::function<bool()>;
        std::shared_ptr<Function> function_;
    public:
        SourceTransferCondition() noexcept = default;
        SourceTransferCondition(std::nullptr_t) noexcept {}
        SourceTransferCondition(const SourceTransferCondition&) = default;
        SourceTransferCondition& operator=(const SourceTransferCondition&) = default;
        SourceTransferCondition(SourceTransferCondition&& other) noexcept
            : SourceTransferCondition(static_cast<const SourceTransferCondition&>(other)) {}
        SourceTransferCondition& operator=(SourceTransferCondition&& other) noexcept {
            return operator=(static_cast<const SourceTransferCondition&>(other));
        }
        template<class F> requires (!std::is_same_v<std::remove_cvref_t<F>, SourceTransferCondition> &&
            std::is_constructible_v<Function, F&&>)
        SourceTransferCondition(F&& callable) {
            Function function(std::forward<F>(callable));
            if (function) function_ = std::make_shared<Function>(std::move(function));
        }
        explicit operator bool() const noexcept { return function_ && static_cast<bool>(*function_); }
        bool operator()() const {
            const auto function = function_;
            if (!function || !*function) d_runtime::ngFailNullAccess(false);
            return (*function)();
        }
    };
    virtual void _applyDeformToChildren(Node::FilterHook filterChildren,
        SourceUpdate update, SourceTransferCondition transferCondition,
        common::SharedArray<std::shared_ptr<Parameter>> params, bool recursive = true);

protected:
    void applyDeformToChildrenInternal(
        const std::shared_ptr<Node>& self,
        const Node::DeformFilterHook::Func& filterChildren,
        const std::function<void(const Vec2Array&)>& update,
        const std::function<bool()>& transferCondition,
        common::SharedArray<std::shared_ptr<Parameter>> params,
        bool recursive);
};

// CRTP mixin: assume the derived is also a Node (as in D mixin template)
template <typename Derived>
class NodeFilterMixin : public virtual NodeFilter {
public:
    void dispose() override {
        auto* self = static_cast<Derived*>(this);
        static_assert(std::is_convertible_v<Derived*, Node*>, "NodeFilterMixin requires public Node conversion");
        [[maybe_unused]] const auto owner = self->weak_from_this().lock();
        const auto children = self->childrenList();
        for (auto child : children) self->releaseChild(child);
        self->childrenRef().resize(0);
    }
};

} // namespace nicxlive::core::nodes
