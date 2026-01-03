#include "filter.hpp"

namespace nicxlive::core::nodes {

void NodeFilter::captureTarget(const std::shared_ptr<Node>&) {}
void NodeFilter::releaseTarget(const std::shared_ptr<Node>&) {}
void NodeFilter::dispose() {}
void NodeFilter::applyDeformToChildren(const std::vector<std::shared_ptr<Parameter>>&, bool) {}

template <typename Derived>
void NodeFilterMixin<Derived>::dispose() {
    auto self = static_cast<Derived*>(this);
    static_assert(std::is_base_of_v<Node, Derived>, "NodeFilterMixin requires Node base");
    auto& children = self->childrenRef();
    for (auto& c : children) {
        self->releaseChild(c);
    }
    children.clear();
}

template <typename Derived>
void NodeFilterMixin<Derived>::applyDeformToChildren(const std::vector<std::shared_ptr<Parameter>>& params, bool recursive) {
    auto self = static_cast<Derived*>(this);
    for (auto& param : params) {
        if (!param) continue;
        auto deformBinding = std::dynamic_pointer_cast<core::param::DeformationParameterBinding>(param->getBinding(self->shared_from_this(), "deform"));
        if (!deformBinding) continue;
        auto xCount = param->axisPointCount(0);
        auto yCount = param->axisPointCount(1);
        if (yCount == 0) yCount = 1;
        for (std::size_t x = 0; x < xCount; ++x) {
            for (std::size_t y = 0; y < yCount; ++y) {
                core::param::Vec2u key{x, y};
                nodes::Vec2 ofs{0, 0};
                if (x + 1 == xCount && x > 0) { key.x = x - 1; ofs.x = 1; }
                if (param->isVec2 && y + 1 == yCount && y > 0) { key.y = y - 1; ofs.y = 1; }
                core::param::DeformSlot slot;
                if (deformBinding->isSetAt(core::param::Vec2u{x, y})) {
                    slot = deformBinding->valueAt(core::param::Vec2u{x, y});
                } else {
                    slot = deformBinding->sample(key, ofs);
                }
                auto applySlot = [&](const std::shared_ptr<Node>& node, auto&& applySlotRef) -> void {
                    if (!node) return;
                    if (slot.vertexOffsets.size() > 0) {
                        node->setValue("transform.t.x", slot.vertexOffsets.x[0]);
                        node->setValue("transform.t.y", slot.vertexOffsets.y[0]);
                    }
                    if (recursive) {
                        for (auto& c : node->childrenRef()) {
                            applySlotRef(c, applySlotRef);
                        }
                    }
                };
                for (auto& child : self->childrenRef()) {
                    applySlot(child, applySlot);
                }
            }
        }
    }
}

// Explicit template instantiations for common derived types can go here if needed.

} // namespace nicxlive::core::nodes
