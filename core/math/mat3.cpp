#include "mat3.hpp"
#include "inmath_float.hpp"

namespace nicxlive::core::math {

Mat3x3 multiply(const Mat3x3& a, const Mat3x3& b) {
    Mat3x3 out{};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            float value = 0.0f;
            for (int k = 0; k < 3; ++k) {
                value = detail::inmathAccumulate(value, detail::inmathProduct(a.m[r][k], b.m[k][c]));
            }
            out.m[r][c] = value;
        }
    }
    return out;
}

Mat3x3 inverse(const Mat3x3& m) {
    Mat3x3 inv{};
    float det = m.m[0][0] * (m.m[1][1] * m.m[2][2] - m.m[2][1] * m.m[1][2]) -
                m.m[0][1] * (m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0]) +
                m.m[0][2] * (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]);
    if (det == 0.0f) return Mat3x3{};
    float invDet = 1.0f / det;
    inv.m[0][0] = (m.m[1][1] * m.m[2][2] - m.m[2][1] * m.m[1][2]) * invDet;
    inv.m[0][1] = (m.m[0][2] * m.m[2][1] - m.m[0][1] * m.m[2][2]) * invDet;
    inv.m[0][2] = (m.m[0][1] * m.m[1][2] - m.m[0][2] * m.m[1][1]) * invDet;
    inv.m[1][0] = (m.m[1][2] * m.m[2][0] - m.m[1][0] * m.m[2][2]) * invDet;
    inv.m[1][1] = (m.m[0][0] * m.m[2][2] - m.m[0][2] * m.m[2][0]) * invDet;
    inv.m[1][2] = (m.m[1][0] * m.m[0][2] - m.m[0][0] * m.m[1][2]) * invDet;
    inv.m[2][0] = (m.m[1][0] * m.m[2][1] - m.m[2][0] * m.m[1][1]) * invDet;
    inv.m[2][1] = (m.m[2][0] * m.m[0][1] - m.m[0][0] * m.m[2][1]) * invDet;
    inv.m[2][2] = (m.m[0][0] * m.m[1][1] - m.m[1][0] * m.m[0][1]) * invDet;
    return inv;
}

Vec2 applyAffine(const Mat3x3& m, const Vec2& p) {
    Vec2 out{};
    out.x = m.m[0][0] * p.x + m.m[0][1] * p.y + m.m[0][2];
    out.y = m.m[1][0] * p.x + m.m[1][1] * p.y + m.m[1][2];
    return out;
}

} // namespace nicxlive::core::math
