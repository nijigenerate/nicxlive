#pragma once

#include "filter.hpp"
#include "common.hpp"

namespace nicxlive::core::nodes {

using ::nicxlive::core::common::Vec2Array;

struct DeformResult {
    Vec2Array vertices{};
    Mat4* transform{nullptr};
    bool changed{false};
};

class Deformer : public NodeFilter {
public:
    ~Deformer() override = default;

    // D版: deformChildren(target, vertices, deformation, origTransform) -> (Vec2Array, mat4*, bool)
    virtual DeformResult deformChildren(const std::shared_ptr<Node>& target,
                                        const Vec2Array& origVertices,
                                        Vec2Array origDeformation,
                                        const Mat4* origTransform) = 0;

    virtual void clearCache() = 0;
};

} // namespace nicxlive::core::nodes
