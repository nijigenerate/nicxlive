#pragma once

#include "filter.hpp"
#include "common.hpp"

#include <optional>
#include <tuple>
#include <vector>

namespace nicxlive::core::nodes {

using ::nicxlive::core::common::Vec2Array;

struct DeformResult {
    std::vector<Vec2> vertices{};
    std::optional<Mat4> transform{};
    bool changed{false};
};

class Deformer : public NodeFilter {
public:
    ~Deformer() override = default;

    // D版: deformChildren(target, vertices, deformation, origTransform) -> (Vec2Array, mat4*, bool)
    virtual DeformResult deformChildren(const std::shared_ptr<Node>& target,
                                        const std::vector<Vec2>& origVertices,
                                        const std::vector<Vec2>& origDeformation,
                                        const Mat4* origTransform) = 0;

    virtual void clearCache() = 0;
};

} // namespace nicxlive::core::nodes
