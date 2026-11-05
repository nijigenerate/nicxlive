#pragma once

#include "filter.hpp"
#include "common.hpp"
#include <tuple>

namespace nicxlive::core::nodes {

using ::nicxlive::core::common::Vec2Array;

using DeformResult = Node::DeformFilterResult;
// The original value-descriptor result; independent of the legacy native result.
using SourceDeformResult = std::tuple<Vec2Array, Mat4*, bool>;

// Complete original Source Deformer contract, including its NodeFilter parent.
// Virtual inheritance shares one actual filter with legacy-compatible nodes.
class SourceDeformation : public virtual NodeFilter {
public:
    virtual ~SourceDeformation() = default;
    virtual SourceDeformResult deformChildrenSource(std::shared_ptr<Node> target,
        Vec2Array originalVertices, Vec2Array originalDeformation, Mat4* originalTransform) = 0;
    virtual void clearCache() = 0;
};
using SourceDeformer = SourceDeformation;

class Deformer : public virtual NodeFilter {
public:
    ~Deformer() override = default;

    // D版: deformChildren(target, vertices, deformation, origTransform) -> (Vec2Array, mat4*, bool)
    virtual DeformResult deformChildren(const std::shared_ptr<Node>& target,
                                        const Vec2Array& origVertices,
                                        Vec2Array& origDeformation,
                                        const Mat4* origTransform) = 0;

    virtual void clearCache() = 0;
};

} // namespace nicxlive::core::nodes
