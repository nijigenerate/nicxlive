#pragma once

#include "filter.hpp"
#include "common.hpp"

#include <tuple>
#include <vector>

namespace nicxlive::core::nodes {

struct DeformResult {
    std::vector<Vec2> vertices{};
    std::optional<Mat4> transform{};
    bool changed{false};
};

class Deformer : public NodeFilter {
public:
    ~Deformer() override = default;

    virtual DeformResult deformChildren(const std::shared_ptr<Node>& /*target*/,
                                        const std::vector<Vec2>& /*origVertices*/,
                                        const std::vector<Vec2>& /*origDeformation*/,
                                        const Mat4* /*origTransform*/) {
        return {};
    }

    virtual void clearCache() {}
};

} // namespace nicxlive::core::nodes
