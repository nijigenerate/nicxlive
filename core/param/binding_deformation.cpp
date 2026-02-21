#include "parameter.hpp"

#include "../nodes/deformable.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace nicxlive::core::param {

bool pushDeformationToNode(const std::shared_ptr<Node>& node, const DeformSlot& value) {
    if (!node) return false;
    auto deformable = std::dynamic_pointer_cast<::nicxlive::core::nodes::Deformable>(node);
    if (!deformable) return false;
    if (node->uuid == 1022832785u ||
        node->uuid == 1173732246u ||
        node->uuid == 305080986u ||
        node->uuid == 4079733156u) {
        float d0x = 0.0f;
        float d0y = 0.0f;
        float maxAbs = 0.0f;
        std::size_t firstNz = static_cast<std::size_t>(-1);
        if (value.vertexOffsets.size() > 0) {
            d0x = value.vertexOffsets.x[0];
            d0y = value.vertexOffsets.y[0];
            for (std::size_t i = 0; i < value.vertexOffsets.size(); ++i) {
                const float ax = std::fabs(value.vertexOffsets.x[i]);
                const float ay = std::fabs(value.vertexOffsets.y[i]);
                if (firstNz == static_cast<std::size_t>(-1) && (ax > 1e-6f || ay > 1e-6f)) {
                    firstNz = i;
                }
                maxAbs = std::max(maxAbs, ax);
                maxAbs = std::max(maxAbs, ay);
            }
        }
        std::fprintf(stderr,
                     "[nicxlive] bind-deform uuid=%u name=%s inSize=%zu vertSize=%zu d0=(%g,%g) maxAbs=%g firstNz=%zu\n",
                     node->uuid,
                     node->name.c_str(),
                     value.vertexOffsets.size(),
                     deformable->vertices.size(),
                     d0x, d0y, maxAbs, firstNz);
    }
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
