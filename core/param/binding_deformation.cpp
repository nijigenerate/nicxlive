#include "parameter.hpp"

#include "../nodes/deformable.hpp"
#include "../math/serialization.hpp"
#include <algorithm>

namespace nicxlive::core::param {

bool pushDeformationToNode(const std::shared_ptr<Node>& node, const DeformSlot& value) {
    if (!node) return false;
    auto deformable = std::dynamic_pointer_cast<::nicxlive::core::nodes::Deformable>(node);
    if (!deformable) return false;
    ::nicxlive::core::nodes::Deformation deform(value);
    deformable->deformStack.push(deform);
    return true;
}

bool getDeformationNodeVertexCount(const std::shared_ptr<Node>& node, std::size_t& outVertexCount) {
    outVertexCount = 0;
    if (!node) return false;
    auto deformable = std::dynamic_pointer_cast<::nicxlive::core::nodes::Deformable>(node);
    if (!deformable) return false;
    outVertexCount = deformable->getVertices().size();
    return true;
}


std::size_t recoverDeformationOffsetsForNode(const std::shared_ptr<Node>& node, Vec2Array& offsets,
                                           std::span<const std::uint8_t> missing) {
    auto deformable = std::dynamic_pointer_cast<::nicxlive::core::nodes::Deformable>(node);
    if (!deformable) return 0;
    return ::nicxlive::core::math::inRecoverDeformationOffsets(deformable->getVertices(), offsets, missing);
}

bool isDeformationNode(const std::shared_ptr<Node>& node) {
    return static_cast<bool>(std::dynamic_pointer_cast<nodes::Deformable>(node));
}

void resetBindingDeformationForNode(std::shared_ptr<Node> node, DeformSlot& value) {
    const auto deformable = std::dynamic_pointer_cast<nodes::Deformable>(node);
    if (!deformable) return;
    value.vertexOffsets.resize(deformable->getVertices().size());
    const auto count = deformable->getVertices().size();
    for (std::size_t i = 0; i < count; ++i) value.vertexOffsets.set(i, Vec2{0, 0});
}

std::size_t recoverBindingDeformationOffsetsForNode(std::shared_ptr<Node> node,
        common::SharedArray<common::SharedArray<DeformSlot>>& values, std::size_t x, std::size_t y,
        common::SharedArray<std::uint8_t> missing) {
    const auto deformable = std::dynamic_pointer_cast<nodes::Deformable>(node);
    const auto vertices = d_runtime::ngRequireObject(deformable)->getVertices();
    auto row = values[x];
    return math::inRecoverDeformationOffsets(vertices, row[y].vertexOffsets, missing);
}

} // namespace nicxlive::core::param
