#pragma once

#include "mat4.hpp"
#include "mat3.hpp"
#include <cmath>

namespace nicxlive::core::math {

struct Transform {
    Vec3 translation{0.0f, 0.0f, 0.0f};
    Vec3 rotation{0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f}; // D 版は vec2。Z は 1 固定運用。
    bool pixelSnap{false};

    // 行列キャッシュ
    Mat4 trs{Mat4::identity()};

    Transform() = default;
    Transform(const Vec3& t, const Vec3& r = Vec3{0.0f, 0.0f, 0.0f}, const Vec2& s = Vec2{1.0f, 1.0f})
        : translation(t), rotation(r), scale{ s.x, s.y, 1.0f } {
        update();
    }

    static Transform identity() { return Transform{}; }

    void clear() {
        translation = Vec3{0.0f, 0.0f, 0.0f};
        rotation = Vec3{0.0f, 0.0f, 0.0f};
        scale = Vec3{1.0f, 1.0f, 1.0f};
        pixelSnap = false;
        trs = Mat4::identity();
    }

    Mat4 toMat4() const { return trs; }
    Mat4 matrix() const { return trs; }

    void update() {
        Vec3 snapped = translation;
        if (pixelSnap) {
            snapped.x = std::round(snapped.x);
            snapped.y = std::round(snapped.y);
            snapped.z = std::round(snapped.z);
        }
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

        Mat4 t = Mat4::translation(snapped);
        Mat4 s = Mat4::scale(scale);
        trs = Mat4::multiply(t, Mat4::multiply(rot, s));
    }

    Transform calcOffset(const Transform& other) const {
        Transform out;
        out.translation = Vec3{translation.x + other.translation.x,
                               translation.y + other.translation.y,
                               translation.z + other.translation.z};
        out.rotation = Vec3{rotation.x + other.rotation.x,
                            rotation.y + other.rotation.y,
                            rotation.z + other.rotation.z};
        out.scale = Vec3{scale.x * other.scale.x,
                         scale.y * other.scale.y,
                         scale.z * other.scale.z};
        out.pixelSnap = pixelSnap || other.pixelSnap;
        out.update();
        return out;
    }

    Transform operator*(const Transform& other) const {
        Transform tnew;
        Mat4 strs = Mat4::multiply(other.trs, trs);
        tnew.translation = Vec3{strs.a.a[0][3], strs.a.a[1][3], strs.a.a[2][3]};
        tnew.rotation = Vec3{rotation.x + other.rotation.x,
                             rotation.y + other.rotation.y,
                             rotation.z + other.rotation.z};
        tnew.scale = Vec3{scale.x * other.scale.x,
                          scale.y * other.scale.y,
                          scale.z * other.scale.z};
        tnew.pixelSnap = pixelSnap || other.pixelSnap;
        tnew.trs = strs;
        return tnew;
    }
};

struct Transform2D {
    Vec2 translation{0.0f, 0.0f};
    Vec2 scale{1.0f, 1.0f};
    float rotation{0.0f};
    Mat3x3 trs{};

    Mat3x3 matrix() const { return trs; }

    void update() {
        Mat3x3 translationM{};
        translationM.m[0][2] = translation.x;
        translationM.m[1][2] = translation.y;

        float c = std::cos(rotation);
        float s = std::sin(rotation);
        Mat3x3 rotationM{};
        rotationM.m[0][0] = c;  rotationM.m[0][1] = -s; rotationM.m[0][2] = 0.0f;
        rotationM.m[1][0] = s;  rotationM.m[1][1] =  c; rotationM.m[1][2] = 0.0f;
        rotationM.m[2][0] = 0.0f; rotationM.m[2][1] = 0.0f; rotationM.m[2][2] = 1.0f;

        Mat3x3 scaleM{};
        scaleM.m[0][0] = scale.x;
        scaleM.m[1][1] = scale.y;
        scaleM.m[2][2] = 1.0f;

        trs = multiply(translationM, multiply(rotationM, scaleM));
    }
};

} // namespace nicxlive::core::math
