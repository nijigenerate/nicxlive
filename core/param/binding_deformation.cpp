#include "parameter.hpp"

#include "../nodes/deformable.hpp"
#include <algorithm>

namespace nicxlive::core::param {

bool pushDeformationToNode(const std::shared_ptr<Node>& node, const DeformSlot& value) {
    if (!node) return false;
    auto deformable = std::dynamic_pointer_cast<::nicxlive::core::nodes::Deformable>(node);
    if (!deformable) return false;
    ::nicxlive::core::nodes::Deformation deform;
    deform.vertexOffsets = value.vertexOffsets;
    deformable->deformStack.push(deform);
    return true;
}

bool getDeformationNodeVertexCount(const std::shared_ptr<Node>& node, std::size_t& outVertexCount) {
    outVertexCount = 0;
    if (!node) return false;
    auto deformable = std::dynamic_pointer_cast<::nicxlive::core::nodes::Deformable>(node);
    if (!deformable) return false;
    outVertexCount = deformable->vertices.size();
    return true;
}

} // namespace nicxlive::core::param
