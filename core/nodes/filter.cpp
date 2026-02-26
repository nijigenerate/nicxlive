#include "filter.hpp"
#include "composite.hpp"
#include "deformable.hpp"
#include "../puppet.hpp"

#include <unordered_map>

namespace nicxlive::core::nodes {

void NodeFilter::captureTarget(const std::shared_ptr<Node>&) {}
void NodeFilter::releaseTarget(const std::shared_ptr<Node>&) {}
void NodeFilter::dispose() {}
void NodeFilter::applyDeformToChildren(const std::vector<std::shared_ptr<Parameter>>&, bool) {}

void NodeFilter::applyDeformToChildrenInternal(
    const std::shared_ptr<Node>& self,
    const Node::FilterHook::Func& filterChildren,
    const std::function<void(const Vec2Array&)>& update,
    const std::function<bool()>& transferCondition,
    const std::vector<std::shared_ptr<Parameter>>& params,
    bool recursive) {
    if (!self) return;

    static constexpr const char* kTransformBindingNames[] = {
        "transform.t.x",
        "transform.t.y",
        "transform.r.z",
        "transform.s.x",
        "transform.s.y",
    };

    for (const auto& param : params) {
        if (!param) continue;

        std::unordered_map<std::string, std::unordered_map<uint32_t, std::shared_ptr<core::param::ParameterBinding>>> trsBindings;
        for (const auto& kv : param->bindingMap) {
            const auto& binding = kv.second;
            if (!binding) continue;
            const auto target = binding->getTarget();
            trsBindings[target.name][target.uuid] = binding;
        }

        auto applyTranslation = [&](const std::shared_ptr<Node>& node, const core::param::Vec2u& keypoint, const Vec2& ofs) {
            if (!node) return;
            for (const char* name : kTransformBindingNames) {
                auto byName = trsBindings.find(name);
                if (byName == trsBindings.end()) continue;
                auto byNode = byName->second.find(node->uuid);
                if (byNode == byName->second.end()) continue;
                if (!byNode->second) continue;
                byNode->second->apply(keypoint, ofs);
            }
        };

        auto binding = param->getBinding(self, "deform");
        auto deformBinding = std::dynamic_pointer_cast<core::param::DeformationParameterBinding>(binding);
        if (!deformBinding) continue;

        const std::size_t xCount = param->axisPointCount(0);
        const std::size_t yCount = param->axisPointCount(1);
        if (xCount == 0 || yCount == 0) continue;

        auto resetOffset = [&](const std::shared_ptr<Node>& node, const auto& selfRef) -> void {
            if (!node) return;
            node->offsetTransform.clear();
            node->offsetSort = 0.0f;
            node->transformChanged();
            for (auto& child : node->childrenRef()) {
                selfRef(child, selfRef);
            }
        };

        std::function<void(const std::shared_ptr<Node>&, int, int)> transferChildrenImpl;
        transferChildrenImpl = [&](const std::shared_ptr<Node>& node, int x, int y) {
            if (!node) return;
            auto deformable = std::dynamic_pointer_cast<Deformable>(node);
            auto composite = std::dynamic_pointer_cast<Composite>(node);
            const bool isComposite = static_cast<bool>(composite);
            const bool mustPropagate = node->mustPropagate();

            if (deformable) {
                int xx = x;
                int yy = y;
                float ofsX = 0.0f;
                float ofsY = 0.0f;
                if (x == static_cast<int>(xCount) - 1) {
                    xx = x - 1;
                    ofsX = 1.0f;
                }
                if (y == static_cast<int>(yCount) - 1) {
                    yy = y - 1;
                    ofsY = 1.0f;
                }

                applyTranslation(node, core::param::Vec2u{
                    static_cast<std::size_t>(xx),
                    static_cast<std::size_t>(yy),
                }, Vec2{ofsX, ofsY});
                node->transformChanged();

                auto nodeBinding = std::dynamic_pointer_cast<core::param::DeformationParameterBinding>(
                    param->getOrAddBinding(node, "deform"));
                if (nodeBinding) {
                    const core::param::Vec2u point{
                        static_cast<std::size_t>(x),
                        static_cast<std::size_t>(y),
                    };
                    Vec2Array nodeDeform = nodeBinding->valueAt(point).vertexOffsets.dup();
                    Mat4 matrix = node->transform().toMat4();
                    auto filterResult = filterChildren(node, deformable->vertices, nodeDeform, &matrix);
                    if (!std::get<0>(filterResult).empty()) {
                        nodeBinding->setRawOffsetsAt(point, std::get<0>(filterResult));
                    }
                }
            } else if (transferCondition() && !isComposite) {
                Vec2Array vertices;
                vertices.resize(1);
                vertices.xAt(0) = node->localTransform.translation.x;
                vertices.yAt(0) = node->localTransform.translation.y;

                auto parent = node->parentPtr();
                Mat4 matrix = parent ? parent->transform().toMat4() : Mat4::identity();

                auto nodeBindingX = std::dynamic_pointer_cast<core::param::ValueParameterBinding>(
                    param->getOrAddBinding(node, "transform.t.x"));
                auto nodeBindingY = std::dynamic_pointer_cast<core::param::ValueParameterBinding>(
                    param->getOrAddBinding(node, "transform.t.y"));

                Vec2Array nodeDeform;
                nodeDeform.resize(1);
                nodeDeform.xAt(0) = node->offsetTransform.translation.x;
                nodeDeform.yAt(0) = node->offsetTransform.translation.y;

                auto filterResult = filterChildren(node, vertices, nodeDeform, &matrix);
                const auto& filtered = std::get<0>(filterResult);
                if (!filtered.empty() && nodeBindingX && nodeBindingY) {
                    const core::param::Vec2u point{
                        static_cast<std::size_t>(x),
                        static_cast<std::size_t>(y),
                    };
                    nodeBindingX->setRawValueAt(point, nodeBindingX->valueAt(point) + filtered.xAt(0));
                    nodeBindingY->setRawValueAt(point, nodeBindingY->valueAt(point) + filtered.yAt(0));
                }
            }

            if (recursive && mustPropagate) {
                for (auto& child : node->childrenRef()) {
                    transferChildrenImpl(child, x, y);
                }
            }
        };

        for (int x = 0; x < static_cast<int>(xCount); ++x) {
            for (int y = 0; y < static_cast<int>(yCount); ++y) {
                if (auto pup = self->puppetRef()) {
                    if (pup->root) resetOffset(pup->root, resetOffset);
                }

                const bool rightMost = x == static_cast<int>(xCount) - 1;
                const bool bottomMost = y == static_cast<int>(yCount) - 1;
                const core::param::Vec2u leftKey{
                    rightMost ? static_cast<std::size_t>(x - 1) : static_cast<std::size_t>(x),
                    bottomMost ? static_cast<std::size_t>(y - 1) : static_cast<std::size_t>(y),
                };
                const Vec2 ofs{
                    rightMost ? 1.0f : 0.0f,
                    bottomMost ? 1.0f : 0.0f,
                };
                const core::param::Vec2u point{
                    static_cast<std::size_t>(x),
                    static_cast<std::size_t>(y),
                };

                Vec2Array deformation;
                if (deformBinding->isSetAt(point)) {
                    deformation = deformBinding->valueAt(point).vertexOffsets;
                } else {
                    deformation = deformBinding->sample(leftKey, ofs).vertexOffsets;
                }
                update(deformation);

                for (auto& child : self->childrenRef()) {
                    transferChildrenImpl(child, x, y);
                }
            }
        }

        param->removeBinding(binding);
    }
}

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
    (void)params;
    (void)recursive;
}

// Explicit template instantiations for common derived types can go here if needed.

} // namespace nicxlive::core::nodes
