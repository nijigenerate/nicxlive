#pragma once

#include "../nodes/common.hpp"

#include <vector>

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

} // namespace nicxlive::core::math
