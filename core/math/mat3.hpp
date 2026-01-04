#pragma once

#include "types.hpp"

namespace nicxlive::core::math {

struct Mat3x3 {
    float m[3][3]{
        {1, 0, 0},
        {0, 1, 0},
        {0, 0, 1},
    };
    float* operator[](int r) { return m[r]; }
    const float* operator[](int r) const { return m[r]; }
};

Mat3x3 multiply(const Mat3x3& a, const Mat3x3& b);
Mat3x3 inverse(const Mat3x3& m);
Vec2 applyAffine(const Mat3x3& m, const Vec2& p);

} // namespace nicxlive::core::math
