#pragma once
// Inverse adapted from inmath 1.0.6, David Herberth / Inochi2D Project.
// Copyright (c) 2011, David Herberth. MIT: ../animation/inmath.LICENSE.

#include "types.hpp"
#include "inmath_float.hpp"

#include <boost/qvm/mat.hpp>
#include <boost/qvm/mat_operations.hpp>
#include <cmath>
#include "../compat/assert_error.hpp"

namespace nicxlive::core::math {

struct Mat4 {
    boost::qvm::mat<float, 4, 4> a{};

    float* operator[](int row) { return a.a[row]; }
    const float* operator[](int row) const { return a.a[row]; }

    static Mat4 identity() {
        Mat4 out{};
        out.a = boost::qvm::identity_mat<float, 4>();
        return out;
    }

    static Mat4 orthographic(float left, float right, float bottom, float top, float zNear, float zFar) {
#ifndef NDEBUG
        // Preserve the original ordered in-contract checks and Error hierarchy.
        const auto source = d_runtime::RetainedString::fromStaticLiteral("inmath/linalg.d");
        if (!(right - left != 0)) throw d_runtime::AssertError(d_runtime::RetainedString{}, source, 1395);
        if (!(top - bottom != 0)) throw d_runtime::AssertError(d_runtime::RetainedString{}, source, 1396);
        if (!(zFar - zNear != 0)) throw d_runtime::AssertError(d_runtime::RetainedString{}, source, 1397);
#endif
        const float rl = right - left;
        const float tb = top - bottom;
        const float fn = zFar - zNear;
        Mat4 out{};
        out.a.a[0][0] = 2.0f / rl;
        out.a.a[0][3] = -(right + left) / rl;
        out.a.a[1][1] = 2.0f / tb;
        out.a.a[1][3] = -(top + bottom) / tb;
        out.a.a[2][2] = -2.0f / fn;
        out.a.a[2][3] = -(zFar + zNear) / fn;
        out.a.a[3][3] = 1;
        return out;
    }

    static Mat4 multiply(const Mat4& lhs, const Mat4& rhs) {
        Mat4 out{};
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                float v = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    v = detail::inmathAccumulate(v, detail::inmathProduct(lhs.a.a[r][k], rhs.a.a[k][c]));
                }
                out.a.a[r][c] = v;
            }
        }
        return out;
    }

    static Mat4 zRotation(double radians) {
        Mat4 out = identity();
        // inmath takes D real (binary64 on LDC Windows MSVC), then casts to mt.
        const float c = static_cast<float>(std::cos(radians));
        const float s = static_cast<float>(std::sin(radians));
        out.a.a[0][0] = c;
        out.a.a[0][1] = -s;
        out.a.a[1][0] = s;
        out.a.a[1][1] = c;
        return out;
    }

    static Mat4 translation(const Vec3& t) {
        Mat4 out = identity();
        out.a.a[0][3] = t.x;
        out.a.a[1][3] = t.y;
        out.a.a[2][3] = t.z;
        return out;
    }

    static Mat4 translation(float x, float y, float z) {
        return translation(Vec3{x, y, z});
    }

    static Mat4 scale(const Vec3& s) {
        Mat4 out = identity();
        out.a.a[0][0] = s.x;
        out.a.a[1][1] = s.y;
        out.a.a[2][2] = s.z;
        return out;
    }

    static Mat4 inverse(const Mat4& m) {
        // inmath 1.0.6, linalg.d: determinant/cofactor inverse (MIT).
        const auto& matrix = m.a.a;
        const float determinant = (matrix[0][3] * matrix[1][2] * matrix[2][1] * matrix[3][0] - matrix[0][2] * matrix[1][3] * matrix[2][1] * matrix[3][0]
                      - matrix[0][3] * matrix[1][1] * matrix[2][2] * matrix[3][0] + matrix[0][1] * matrix[1][3] * matrix[2][2] * matrix[3][0]
                      + matrix[0][2] * matrix[1][1] * matrix[2][3] * matrix[3][0] - matrix[0][1] * matrix[1][2] * matrix[2][3] * matrix[3][0]
                      - matrix[0][3] * matrix[1][2] * matrix[2][0] * matrix[3][1] + matrix[0][2] * matrix[1][3] * matrix[2][0] * matrix[3][1]
                      + matrix[0][3] * matrix[1][0] * matrix[2][2] * matrix[3][1] - matrix[0][0] * matrix[1][3] * matrix[2][2] * matrix[3][1]
                      - matrix[0][2] * matrix[1][0] * matrix[2][3] * matrix[3][1] + matrix[0][0] * matrix[1][2] * matrix[2][3] * matrix[3][1]
                      + matrix[0][3] * matrix[1][1] * matrix[2][0] * matrix[3][2] - matrix[0][1] * matrix[1][3] * matrix[2][0] * matrix[3][2]
                      - matrix[0][3] * matrix[1][0] * matrix[2][1] * matrix[3][2] + matrix[0][0] * matrix[1][3] * matrix[2][1] * matrix[3][2]
                      + matrix[0][1] * matrix[1][0] * matrix[2][3] * matrix[3][2] - matrix[0][0] * matrix[1][1] * matrix[2][3] * matrix[3][2]
                      - matrix[0][2] * matrix[1][1] * matrix[2][0] * matrix[3][3] + matrix[0][1] * matrix[1][2] * matrix[2][0] * matrix[3][3]
                      + matrix[0][2] * matrix[1][0] * matrix[2][1] * matrix[3][3] - matrix[0][0] * matrix[1][2] * matrix[2][1] * matrix[3][3]
                      - matrix[0][1] * matrix[1][0] * matrix[2][2] * matrix[3][3] + matrix[0][0] * matrix[1][1] * matrix[2][2] * matrix[3][3]);
        const float d = 1.0f / determinant;
        Mat4 result;
        result.a.a[0][0] = (matrix[1][1] * matrix[2][2] * matrix[3][3] + matrix[1][2] * matrix[2][3] * matrix[3][1] + matrix[1][3] * matrix[2][1] * matrix[3][2]
                              - matrix[1][1] * matrix[2][3] * matrix[3][2] - matrix[1][2] * matrix[2][1] * matrix[3][3] - matrix[1][3] * matrix[2][2] * matrix[3][1]) * d;
        result.a.a[0][1] = (matrix[0][1] * matrix[2][3] * matrix[3][2] + matrix[0][2] * matrix[2][1] * matrix[3][3] + matrix[0][3] * matrix[2][2] * matrix[3][1]
                              - matrix[0][1] * matrix[2][2] * matrix[3][3] - matrix[0][2] * matrix[2][3] * matrix[3][1] - matrix[0][3] * matrix[2][1] * matrix[3][2]) * d;
        result.a.a[0][2] = (matrix[0][1] * matrix[1][2] * matrix[3][3] + matrix[0][2] * matrix[1][3] * matrix[3][1] + matrix[0][3] * matrix[1][1] * matrix[3][2]
                              - matrix[0][1] * matrix[1][3] * matrix[3][2] - matrix[0][2] * matrix[1][1] * matrix[3][3] - matrix[0][3] * matrix[1][2] * matrix[3][1]) * d;
        result.a.a[0][3] = (matrix[0][1] * matrix[1][3] * matrix[2][2] + matrix[0][2] * matrix[1][1] * matrix[2][3] + matrix[0][3] * matrix[1][2] * matrix[2][1]
                              - matrix[0][1] * matrix[1][2] * matrix[2][3] - matrix[0][2] * matrix[1][3] * matrix[2][1] - matrix[0][3] * matrix[1][1] * matrix[2][2]) * d;
        result.a.a[1][0] = (matrix[1][0] * matrix[2][3] * matrix[3][2] + matrix[1][2] * matrix[2][0] * matrix[3][3] + matrix[1][3] * matrix[2][2] * matrix[3][0]
                              - matrix[1][0] * matrix[2][2] * matrix[3][3] - matrix[1][2] * matrix[2][3] * matrix[3][0] - matrix[1][3] * matrix[2][0] * matrix[3][2]) * d;
        result.a.a[1][1] = (matrix[0][0] * matrix[2][2] * matrix[3][3] + matrix[0][2] * matrix[2][3] * matrix[3][0] + matrix[0][3] * matrix[2][0] * matrix[3][2]
                              - matrix[0][0] * matrix[2][3] * matrix[3][2] - matrix[0][2] * matrix[2][0] * matrix[3][3] - matrix[0][3] * matrix[2][2] * matrix[3][0]) * d;
        result.a.a[1][2] = (matrix[0][0] * matrix[1][3] * matrix[3][2] + matrix[0][2] * matrix[1][0] * matrix[3][3] + matrix[0][3] * matrix[1][2] * matrix[3][0]
                              - matrix[0][0] * matrix[1][2] * matrix[3][3] - matrix[0][2] * matrix[1][3] * matrix[3][0] - matrix[0][3] * matrix[1][0] * matrix[3][2]) * d;
        result.a.a[1][3] = (matrix[0][0] * matrix[1][2] * matrix[2][3] + matrix[0][2] * matrix[1][3] * matrix[2][0] + matrix[0][3] * matrix[1][0] * matrix[2][2]
                              - matrix[0][0] * matrix[1][3] * matrix[2][2] - matrix[0][2] * matrix[1][0] * matrix[2][3] - matrix[0][3] * matrix[1][2] * matrix[2][0]) * d;
        result.a.a[2][0] = (matrix[1][0] * matrix[2][1] * matrix[3][3] + matrix[1][1] * matrix[2][3] * matrix[3][0] + matrix[1][3] * matrix[2][0] * matrix[3][1]
                              - matrix[1][0] * matrix[2][3] * matrix[3][1] - matrix[1][1] * matrix[2][0] * matrix[3][3] - matrix[1][3] * matrix[2][1] * matrix[3][0]) * d;
        result.a.a[2][1] = (matrix[0][0] * matrix[2][3] * matrix[3][1] + matrix[0][1] * matrix[2][0] * matrix[3][3] + matrix[0][3] * matrix[2][1] * matrix[3][0]
                              - matrix[0][0] * matrix[2][1] * matrix[3][3] - matrix[0][1] * matrix[2][3] * matrix[3][0] - matrix[0][3] * matrix[2][0] * matrix[3][1]) * d;
        result.a.a[2][2] = (matrix[0][0] * matrix[1][1] * matrix[3][3] + matrix[0][1] * matrix[1][3] * matrix[3][0] + matrix[0][3] * matrix[1][0] * matrix[3][1]
                              - matrix[0][0] * matrix[1][3] * matrix[3][1] - matrix[0][1] * matrix[1][0] * matrix[3][3] - matrix[0][3] * matrix[1][1] * matrix[3][0]) * d;
        result.a.a[2][3] = (matrix[0][0] * matrix[1][3] * matrix[2][1] + matrix[0][1] * matrix[1][0] * matrix[2][3] + matrix[0][3] * matrix[1][1] * matrix[2][0]
                              - matrix[0][0] * matrix[1][1] * matrix[2][3] - matrix[0][1] * matrix[1][3] * matrix[2][0] - matrix[0][3] * matrix[1][0] * matrix[2][1]) * d;
        result.a.a[3][0] = (matrix[1][0] * matrix[2][2] * matrix[3][1] + matrix[1][1] * matrix[2][0] * matrix[3][2] + matrix[1][2] * matrix[2][1] * matrix[3][0]
                              - matrix[1][0] * matrix[2][1] * matrix[3][2] - matrix[1][1] * matrix[2][2] * matrix[3][0] - matrix[1][2] * matrix[2][0] * matrix[3][1]) * d;
        result.a.a[3][1] = (matrix[0][0] * matrix[2][1] * matrix[3][2] + matrix[0][1] * matrix[2][2] * matrix[3][0] + matrix[0][2] * matrix[2][0] * matrix[3][1]
                              - matrix[0][0] * matrix[2][2] * matrix[3][1] - matrix[0][1] * matrix[2][0] * matrix[3][2] - matrix[0][2] * matrix[2][1] * matrix[3][0]) * d;
        result.a.a[3][2] = (matrix[0][0] * matrix[1][2] * matrix[3][1] + matrix[0][1] * matrix[1][0] * matrix[3][2] + matrix[0][2] * matrix[1][1] * matrix[3][0]
                              - matrix[0][0] * matrix[1][1] * matrix[3][2] - matrix[0][1] * matrix[1][2] * matrix[3][0] - matrix[0][2] * matrix[1][0] * matrix[3][1]) * d;
        result.a.a[3][3] = (matrix[0][0] * matrix[1][1] * matrix[2][2] + matrix[0][1] * matrix[1][2] * matrix[2][0] + matrix[0][2] * matrix[1][0] * matrix[2][1]
                              - matrix[0][0] * matrix[1][2] * matrix[2][1] - matrix[0][1] * matrix[1][0] * matrix[2][2] - matrix[0][2] * matrix[1][1] * matrix[2][0]) * d;
        return result;
    }

    Mat4 inverse() const { return Mat4::inverse(*this); }

    Vec3 transformPoint(const Vec3& v) const {
        Vec3 out{};
        out.x = a.a[0][0] * v.x + a.a[0][1] * v.y + a.a[0][2] * v.z + a.a[0][3];
        out.y = a.a[1][0] * v.x + a.a[1][1] * v.y + a.a[1][2] * v.z + a.a[1][3];
        out.z = a.a[2][0] * v.x + a.a[2][1] * v.y + a.a[2][2] * v.z + a.a[2][3];
        return out;
    }
};

static_assert(std::is_standard_layout_v<Mat4>, "Mat4 must be standard layout");
static_assert(sizeof(Mat4) == sizeof(float) * 16, "Mat4 size must be 16 floats");
static_assert(alignof(Mat4) == alignof(float), "Mat4 alignment must match float");

inline Mat4 operator*(const Mat4& lhs, const Mat4& rhs) {
    return Mat4::multiply(lhs, rhs);
}

inline Vec4 operator*(const Mat4& matrix, const Vec4& vector) {
    Vec4 result{};
    const float input[4]{vector.x, vector.y, vector.z, vector.w};
    float* output[4]{&result.x, &result.y, &result.z, &result.w};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            *output[row] = detail::inmathAccumulate(*output[row],
                detail::inmathProduct(matrix.a.a[row][column], input[column]));
        }
    }
    return result;
}

} // namespace nicxlive::core::math
