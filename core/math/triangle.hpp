#pragma once

#include "../nodes/common.hpp"

namespace nicxlive::core::math {

std::array<float, 3> barycentric(const ::nicxlive::core::nodes::Vec2& p,
                                 const ::nicxlive::core::nodes::Vec2& v0,
                                 const ::nicxlive::core::nodes::Vec2& v1,
                                 const ::nicxlive::core::nodes::Vec2& v2);

bool pointInTriangle(const ::nicxlive::core::nodes::Vec2& p,
                     const ::nicxlive::core::nodes::Vec2& v0,
                     const ::nicxlive::core::nodes::Vec2& v1,
                     const ::nicxlive::core::nodes::Vec2& v2);

} // namespace nicxlive::core::math
