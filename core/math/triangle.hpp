#pragma once

#include "../nodes/common.hpp"

#include <vector>
#include "../common/shared_array.hpp"
#include <memory>

namespace nicxlive::core::nodes {
struct MeshData;
}

namespace nicxlive::core::math {

std::array<float, 3> barycentric(const ::nicxlive::core::nodes::Vec2& p,
                                 const ::nicxlive::core::nodes::Vec2& v0,
                                 const ::nicxlive::core::nodes::Vec2& v1,
                                 const ::nicxlive::core::nodes::Vec2& v2);

bool pointInTriangle(const ::nicxlive::core::nodes::Vec2& p,
                     const ::nicxlive::core::nodes::Vec2& v0,
                     const ::nicxlive::core::nodes::Vec2& v1,
                     const ::nicxlive::core::nodes::Vec2& v2);

bool isPointInTriangle(const ::nicxlive::core::nodes::Vec2& pt, const ::nicxlive::core::nodes::Vec2Array& triangle);
std::vector<int> findSurroundingTriangle(const ::nicxlive::core::nodes::Vec2& pt, ::nicxlive::core::nodes::MeshData& bindingMesh);
::nicxlive::core::nodes::Vec2 calcOffsetInTriangleCoords(const ::nicxlive::core::nodes::Vec2& pt,
                                                          ::nicxlive::core::nodes::MeshData& bindingMesh,
                                                          std::vector<int>& triangle);
bool nlCalculateTransformInTriangle(const ::nicxlive::core::nodes::Vec2Array& vertices,
                                    const std::vector<int>& triangle,
                                    const ::nicxlive::core::nodes::Vec2Array& deform,
                                    const ::nicxlive::core::nodes::Vec2& target,
                                    ::nicxlive::core::nodes::Vec2& targetPrime,
                                    float& rotVert,
                                    float& rotHorz);
// Actual Source descriptor entry points; legacy vector APIs above remain available.
bool ngIsPointInTriangleSource(::nicxlive::core::nodes::Vec2 point,
    const ::nicxlive::core::nodes::Vec2Array triangle);
::nicxlive::core::common::SharedArray<int> ngFindSurroundingTriangleSource(
    ::nicxlive::core::nodes::Vec2 point, ::nicxlive::core::nodes::MeshData& mesh);
::nicxlive::core::nodes::Vec2 ngCalcOffsetInTriangleCoordsSource(
    ::nicxlive::core::nodes::Vec2 point,
    ::nicxlive::core::nodes::MeshData& mesh,
    ::nicxlive::core::common::SharedArray<int>& triangle);
::nicxlive::core::common::SharedArray<int> findSurroundingTriangle(
    ::nicxlive::core::nodes::Vec2 point,
    std::shared_ptr<::nicxlive::core::nodes::MeshData> mesh);
bool nlCalculateTransformInTriangle(::nicxlive::core::nodes::Vec2Array vertices,
    ::nicxlive::core::common::SharedArray<int> triangle,
    ::nicxlive::core::nodes::Vec2Array deformation,
    ::nicxlive::core::nodes::Vec2 target,
    ::nicxlive::core::nodes::Vec2& targetPrime, float& rotationVertical, float& rotationHorizontal);

} // namespace nicxlive::core::math
