#pragma once
// Independent nijilive/math/veca_ops.d. BSD-2-Clause, SDK LICENSE.
#include "veca.hpp"
#include "mat4.hpp"
#include "../common/shared_array.hpp"
#include <cstddef>
#include <limits>

namespace nicxlive::core::math {

void ngProjectVec2OntoAxesSource(Vec2Array center, Vec2Array reference,
    Vec2Array axisA, Vec2Array axisB, common::SharedArray<float> outAxisA,
    common::SharedArray<float> outAxisB);
void ngComposeVec2FromAxesSource(Vec2Array& destination, Vec2Array base,
    common::SharedArray<float> axisA, Vec2Array directionA,
    common::SharedArray<float> axisB, Vec2Array directionB);
void ngRotateVec2TangentsToNormalsSource(Vec2Array& normals, Vec2Array tangents);
void ngTransformAssignSource(Vec2Array& destination, Vec2Array source, Mat4 matrix);
void ngTransformAddSource(Vec2Array& destination, Vec2Array source, Mat4 matrix,
    std::size_t count = std::numeric_limits<std::size_t>::max());

} // namespace nicxlive::core::math
