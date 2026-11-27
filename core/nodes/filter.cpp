#include "filter.hpp"
#include "composite.hpp"
#include "deformable.hpp"
#include "../puppet.hpp"

#include <unordered_map>
#include <bit>
#include <cstdint>
#include "../common/string_key_map.hpp"
#include "../compat/assert_error.hpp"

namespace nicxlive::core::nodes {

namespace {
template<class Values> decltype(auto) sourceFilterAt(Values& values, std::size_t index, unsigned line) {
    if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
        index, values.size(), d_runtime::RetainedString::fromStaticLiteral(
            "nijilive/core/nodes/filter.d"), line);
    return values[index];
}

struct FilterResourceHash {
    std::size_t operator()(const std::shared_ptr<Resource>& value) const {
        return ngResourceInterfaceHash(value);
    }
};
struct FilterResourceEqual {
    bool operator()(const std::shared_ptr<Resource>& query, const std::shared_ptr<Resource>& stored) const {
        return ngResourceInterfaceKeyEquals(query, stored);
    }
};

std::int32_t sourceFilterPrevious(std::int32_t value) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value) - 1u);
}
std::int32_t sourceFilterNext(std::int32_t value) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value) + 1u);
}
} // namespace

// Qualified calls from concrete classes implement their actual Source mixin.
void NodeFilter::dispose() {
    auto* node = d_runtime::ngRequireObject(dynamic_cast<Node*>(this));
    [[maybe_unused]] const auto owner = node->weak_from_this().lock();
    const auto children = node->childrenList();
    for (auto child : children) node->releaseChild(child);
    node->childrenRef().resize(0);
}

void NodeFilter::_applyDeformToChildren(Node::FilterHook filterChildren,
    SourceUpdate update, SourceTransferCondition transferCondition,
    common::SharedArray<std::shared_ptr<Parameter>> params, bool recursive) {
    auto* node = d_runtime::ngRequireObject(dynamic_cast<Node*>(this));
    const auto self = node->shared_from_this();
    using Binding = core::param::ParameterBinding;
    using DeformationBinding = core::param::DeformationParameterBinding;
    using ValueBinding = core::param::ValueParameterBinding;
    using DeformationBase = core::param::ParameterBindingImpl<core::param::DeformSlot>;
    using ResourceBindings = common::ResourceKeyMap<std::shared_ptr<Resource>, std::shared_ptr<Binding>,
        FilterResourceHash, FilterResourceEqual>;
    static constexpr std::string_view transformNames[] = {
        "transform.t.x", "transform.t.y", "transform.r.z", "transform.s.x", "transform.s.y"
    };

    for (auto parameter : params) {
        common::StringKeyMap<ResourceBindings> trsBindings;
        const auto bindings = d_runtime::ngRequireObject(parameter)->bindings;
        for (auto binding : bindings) {
            const auto name = d_runtime::ngRequireObject(binding)->getTarget().name;
            const auto key = name.retentionOwner()
                ? d_runtime::RetainedString::retainedView(name.retentionOwner(), name.data(), name.size())
                : d_runtime::RetainedString::fromStaticView(name.view());
            auto outerCell = trsBindings[key];
            const auto target = d_runtime::ngRequireObject(binding)->getTarget().getResource();
            auto& byResource = static_cast<ResourceBindings&>(outerCell);
            auto destination = byResource[target];
            destination = binding;
        }

        auto applyTranslation = [&](std::shared_ptr<Node> target, std::shared_ptr<Parameter> parent,
            core::param::Vec2u keypoint, Vec2 offset) {
            (void)parent;
            for (const auto name : transformNames) {
                if (!trsBindings.contains(name)) continue;
                auto first = trsBindings.get(name);
                if (!first.contains(std::static_pointer_cast<Resource>(target))) continue;
                auto current = trsBindings.get(name);
                const auto entry = current.findRetained(std::static_pointer_cast<Resource>(target));
                if (!entry) throw d_runtime::ngStaticError<d_runtime::RangeError>(
                    d_runtime::RetainedString::fromStaticLiteral("nijilive/core/nodes/filter.d"), 45);
                d_runtime::ngRequireObject(*entry)->apply(keypoint, offset);
            }
        };

        auto transferChildren = [&](auto&& recurse, std::shared_ptr<Node> target,
            std::int32_t x, std::int32_t y) -> void {
            const auto drawable = std::dynamic_pointer_cast<Deformable>(target);
            const auto composite = std::dynamic_pointer_cast<Composite>(target);
            const bool mustPropagate = d_runtime::ngRequireObject(target)->mustPropagate();
            if (drawable) {
                auto xx = x;
                auto yy = y;
                float offsetX = 0;
                float offsetY = 0;
                if (static_cast<std::size_t>(x) == parameter->axisPoints[0].size() - 1u) {
                    xx = sourceFilterPrevious(x);
                    offsetX = 1;
                }
                if (static_cast<std::size_t>(y) == parameter->axisPoints[1].size() - 1u) {
                    yy = sourceFilterPrevious(y);
                    offsetY = 1;
                }
                applyTranslation(drawable, parameter, core::param::Vec2u(xx, yy), Vec2{offsetX, offsetY});
                drawable->transformChanged();
                auto vertices = drawable->getVertices();
                Mat4 matrix = drawable->transform().toMat4();
                auto binding = std::dynamic_pointer_cast<DeformationBinding>(parameter->getOrAddBinding(
                    std::static_pointer_cast<Resource>(target), common::NullableString::fromStaticLiteral("deform")));
                auto& slot = sourceFilterAt(sourceFilterAt(d_runtime::ngRequireObject(binding)->values, x, 76), y, 76);
                auto offsets = slot.vertexOffsets.dup();
                auto result = filterChildren.func(target, vertices, offsets, &matrix);
                if (!std::get<0>(result).empty()) {
                    sourceFilterAt(sourceFilterAt(binding->values, x, 79), y, 79).vertexOffsets = std::get<0>(result);
                    auto& flags = binding->getIsSet();
                    sourceFilterAt(sourceFilterAt(flags, x, 80), y, 80) = true;
                }
            } else {
                if (transferCondition() && !composite) {
                    Vec2Array vertices(Vec2{
                        target->localTransform.translation.x, target->localTransform.translation.y});
                    Mat4 matrix = Mat4::identity();
                    if (target->parentPtr()) {
                        const auto parent = d_runtime::ngRequireObject(target->parentPtr());
                        matrix = parent->transform().toMat4();
                    }
                    auto bindingX = std::dynamic_pointer_cast<ValueBinding>(parameter->getOrAddBinding(
                        std::static_pointer_cast<Resource>(target), common::NullableString::fromStaticLiteral("transform.t.x")));
                    auto bindingY = std::dynamic_pointer_cast<ValueBinding>(parameter->getOrAddBinding(
                        std::static_pointer_cast<Resource>(target), common::NullableString::fromStaticLiteral("transform.t.y")));
                    Vec2Array offsets(Vec2{
                        target->offsetTransform.translation.x, target->offsetTransform.translation.y});
                    auto result = filterChildren.func(target, vertices, offsets, &matrix);
                    if (!std::get<0>(result).empty()) {
                        sourceFilterAt(sourceFilterAt(d_runtime::ngRequireObject(bindingX)->values, x, 91), y, 91)
                            += std::get<0>(result)[0].x;
                        sourceFilterAt(sourceFilterAt(d_runtime::ngRequireObject(bindingY)->values, x, 92), y, 92)
                            += std::get<0>(result)[0].y;
                        auto& flagsX = bindingX->getIsSet();
                        sourceFilterAt(sourceFilterAt(flagsX, x, 93), y, 93) = true;
                        auto& flagsY = bindingY->getIsSet();
                        sourceFilterAt(sourceFilterAt(flagsY, x, 94), y, 94) = true;
                    }
                }
            }
            if (recursive && mustPropagate) {
                const auto children = target->childrenList();
                for (auto child : children) recurse(recurse, child, x, y);
            }
        };

        auto resetOffset = [&](auto&& recurse, std::shared_ptr<Node> target) -> void {
            if (!target) d_runtime::ngFailNullAccess(true);
            target->offsetTransform.clear();
            target->offsetSort = 0;
            target->transformChanged();
            const auto children = target->childrenList();
            for (auto child : children) recurse(recurse, child);
        };

        auto binding = parameter->getBinding(std::static_pointer_cast<Resource>(self),
            common::NullableString::fromStaticLiteral("deform"));
        if (binding) {
            auto deformBinding = std::dynamic_pointer_cast<DeformationBinding>(binding);
#ifndef NDEBUG
            if (!deformBinding) throw d_runtime::ngStaticError<d_runtime::AssertError>(
                "Assertion failure", "nijilive/core/nodes/filter.d", 118);
#endif
            [[maybe_unused]] const auto target = d_runtime::ngRequireObject(deformBinding)->targetNode();
            for (std::int32_t x = 0; static_cast<std::size_t>(x) < parameter->axisPoints[0].size(); x = sourceFilterNext(x)) {
                for (std::int32_t y = 0; static_cast<std::size_t>(y) < parameter->axisPoints[1].size(); y = sourceFilterNext(y)) {
                    resetOffset(resetOffset, d_runtime::ngRequireObject(self->puppetRef())->root);
                    Vec2Array deformation;
                    // The Source reads the field, not the overridable getIsSet method.
                    auto& sourceFlags = deformBinding->DeformationBase::getIsSet();
                    if (sourceFilterAt(sourceFilterAt(sourceFlags, x, 127), y, 127)) {
                        deformation = sourceFilterAt(sourceFilterAt(deformBinding->values, x, 128), y, 128).vertexOffsets;
                    } else {
                        const bool rightMost = static_cast<std::size_t>(x) == parameter->axisPoints[0].size() - 1u;
                        const bool bottomMost = static_cast<std::size_t>(y) == parameter->axisPoints[1].size() - 1u;
                        deformation = deformBinding->interpolate(
                            core::param::Vec2u(rightMost ? sourceFilterPrevious(x) : x,
                                bottomMost ? sourceFilterPrevious(y) : y),
                            Vec2{rightMost ? 1.0f : 0.0f, bottomMost ? 1.0f : 0.0f}).vertexOffsets;
                    }
                    update(deformation);
                    const auto children = self->childrenList();
                    for (auto child : children) transferChildren(transferChildren, child, x, y);
                }
            }
            parameter->removeBinding(binding);
        }
    }
}

void NodeFilter::applyDeformToChildrenInternal(
    const std::shared_ptr<Node>& self,
    const Node::DeformFilterHook::Func& filterChildren,
    const std::function<void(const Vec2Array&)>& update,
    const std::function<bool()>& transferCondition,
    common::SharedArray<std::shared_ptr<Parameter>> params,
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
        for (const auto& binding : param->bindings) {
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
                    auto filterResult = filterChildren(node, deformable->getVertices(), nodeDeform, &matrix);
                    if (filterResult.changed) {
                        nodeBinding->setRawOffsetsAt(point, nodeDeform);
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
                if (filterResult.changed && nodeBindingX && nodeBindingY) {
                    const core::param::Vec2u point{
                        static_cast<std::size_t>(x),
                        static_cast<std::size_t>(y),
                    };
                    nodeBindingX->setRawValueAt(point, nodeBindingX->valueAt(point) + nodeDeform.xAt(0));
                    nodeBindingY->setRawValueAt(point, nodeBindingY->valueAt(point) + nodeDeform.yAt(0));
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

} // namespace nicxlive::core::nodes
