#pragma once

#include "types.hpp"

#include <boost/qvm/mat.hpp>
#include <boost/qvm/mat_operations.hpp>
#include <cmath>

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
        Mat4 out = identity();
        const float rl = right - left;
        const float tb = top - bottom;
        const float fn = zFar - zNear;
        if (rl != 0.0f) {
            out.a.a[0][0] = 2.0f / rl;
            out.a.a[0][3] = -(right + left) / rl;
        }
        if (tb != 0.0f) {
            out.a.a[1][1] = 2.0f / tb;
            out.a.a[1][3] = -(top + bottom) / tb;
        }
        if (fn != 0.0f) {
            out.a.a[2][2] = -2.0f / fn;
            out.a.a[2][3] = -(zFar + zNear) / fn;
        }
        return out;
    }

    static Mat4 multiply(const Mat4& lhs, const Mat4& rhs) {
        Mat4 out{};
        out.a = boost::qvm::operator*(lhs.a, rhs.a);
        return out;
    }

    static Mat4 zRotation(float radians) {
        Mat4 out = identity();
        const float c = std::cos(radians);
        const float s = std::sin(radians);
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
        Mat4 out{};
        out.a = boost::qvm::inverse(m.a);
        return out;
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

inline Mat4 operator*(const Mat4& lhs, const Mat4& rhs) {
    return Mat4::multiply(lhs, rhs);
}

} // namespace nicxlive::core::math
