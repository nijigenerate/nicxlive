#pragma once

#include "node.hpp"
#include "../param/parameter.hpp"

#include <functional>
#include <type_traits>

namespace nicxlive::core::nodes {

using ::nicxlive::core::param::Parameter;

class NodeFilter {
public:
    virtual ~NodeFilter() = default;
    virtual void captureTarget(const std::shared_ptr<Node>& target);
    virtual void releaseTarget(const std::shared_ptr<Node>& target);
    virtual void dispose();
    virtual void applyDeformToChildren(const std::vector<std::shared_ptr<Parameter>>& params, bool recursive = true);

protected:
    void applyDeformToChildrenInternal(
        const std::shared_ptr<Node>& self,
        const Node::FilterHook::Func& filterChildren,
        const std::function<void(const Vec2Array&)>& update,
        const std::function<bool()>& transferCondition,
        const std::vector<std::shared_ptr<Parameter>>& params,
        bool recursive);
};

// CRTP mixin: assume the derived is also a Node (as in D mixin template)
template <typename Derived>
class NodeFilterMixin : public NodeFilter {
public:
    void dispose() override;

    void applyDeformToChildren(const std::vector<std::shared_ptr<Parameter>>& params, bool recursive = true) override;
};

} // namespace nicxlive::core::nodes
