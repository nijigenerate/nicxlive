#pragma once

#include "mat4.hpp"

namespace nicxlive::core::math {

struct Transform {
    Vec3 translation{0.0f, 0.0f, 0.0f};
    Vec3 rotation{0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    static Transform identity() { return Transform{}; }

    void clear() {
        translation = Vec3{0.0f, 0.0f, 0.0f};
        rotation = Vec3{0.0f, 0.0f, 0.0f};
        scale = Vec3{1.0f, 1.0f, 1.0f};
    }

    Mat4 toMat4() const {
        const float cx = std::cos(rotation.x);
        const float sx = std::sin(rotation.x);
        const float cy = std::cos(rotation.y);
        const float sy = std::sin(rotation.y);
        const float cz = std::cos(rotation.z);
        const float sz = std::sin(rotation.z);

        Mat4 rot = Mat4::identity();
        rot.a.a[0][0] = cy * cz;
        rot.a.a[0][1] = cz * sx * sy - cx * sz;
        rot.a.a[0][2] = cx * cz * sy + sx * sz;
        rot.a.a[1][0] = cy * sz;
        rot.a.a[1][1] = cx * cz + sx * sy * sz;
        rot.a.a[1][2] = -cz * sx + cx * sy * sz;
        rot.a.a[2][0] = -sy;
        rot.a.a[2][1] = cy * sx;
        rot.a.a[2][2] = cx * cy;

        Mat4 t = Mat4::translation(translation);
        Mat4 s = Mat4::scale(scale);
        return Mat4::multiply(t, Mat4::multiply(rot, s));
    }

    Transform calcOffset(const Transform& offset) const {
        Transform out = *this;
        out.translation.x += offset.translation.x;
        out.translation.y += offset.translation.y;
        out.translation.z += offset.translation.z;
        out.rotation.x += offset.rotation.x;
        out.rotation.y += offset.rotation.y;
        out.rotation.z += offset.rotation.z;
        out.scale.x *= offset.scale.x;
        out.scale.y *= offset.scale.y;
        out.scale.z *= offset.scale.z;
        return out;
    }

    void update() {}
};

} // namespace nicxlive::core::math
