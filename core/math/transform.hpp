#pragma once

#include "mat4.hpp"
#include "mat3.hpp"
#include "../serde.hpp"

#include <cmath>
#include <exception>
#include <sstream>

namespace nicxlive::core::math {

struct Transform {
    Vec3 translation{0.0f, 0.0f, 0.0f};
    Vec3 rotation{0.0f, 0.0f, 0.0f};
    Vec2 scale{1.0f, 1.0f};
    bool pixelSnap{false};

    // Cached matrix
    Mat4 trs{Mat4::identity()};

    Transform() = default;
    Transform(const Vec3& t, const Vec3& r = Vec3{0.0f, 0.0f, 0.0f}, const Vec2& s = Vec2{1.0f, 1.0f})
        : translation(t), rotation(r), scale(s) {
        update();
    }

    static Transform identity() { return Transform{}; }

    void clear() {
        translation = Vec3{0.0f, 0.0f, 0.0f};
        rotation = Vec3{0.0f, 0.0f, 0.0f};
        scale = Vec2{1.0f, 1.0f};
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
        Mat4 s = Mat4::scale(Vec3{scale.x, scale.y, 1.0f});
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
        out.scale = Vec2{scale.x * other.scale.x,
                         scale.y * other.scale.y};
        out.update();
        return out;
    }

    std::string toString() const {
        std::ostringstream oss;
        oss << "[" << trs.a.a[0][0] << "," << trs.a.a[0][1] << "," << trs.a.a[0][2] << "," << trs.a.a[0][3] << "]\n"
            << "[" << trs.a.a[1][0] << "," << trs.a.a[1][1] << "," << trs.a.a[1][2] << "," << trs.a.a[1][3] << "]\n"
            << "[" << trs.a.a[2][0] << "," << trs.a.a[2][1] << "," << trs.a.a[2][2] << "," << trs.a.a[2][3] << "]\n"
            << "[" << trs.a.a[3][0] << "," << trs.a.a[3][1] << "," << trs.a.a[3][2] << "," << trs.a.a[3][3] << "]\n"
            << "t=(" << translation.x << "," << translation.y << "," << translation.z << ") "
            << "r=(" << rotation.x << "," << rotation.y << "," << rotation.z << ") "
            << "s=(" << scale.x << "," << scale.y << ")";
        return oss.str();
    }

    void serialize(::nicxlive::core::serde::InochiSerializer& serializer) const {
        auto writeVec = [&](const char* key, const float* src, std::size_t n) {
            boost::property_tree::ptree arr;
            for (std::size_t i = 0; i < n; ++i) {
                boost::property_tree::ptree v;
                v.put("", src[i]);
                arr.push_back({"", v});
            }
            serializer.root.add_child(key, arr);
        };
        const float trans[3]{translation.x, translation.y, translation.z};
        const float rot[3]{rotation.x, rotation.y, rotation.z};
        const float scl[2]{scale.x, scale.y};
        writeVec("trans", trans, 3);
        writeVec("rot", rot, 3);
        writeVec("scale", scl, 2);
    }

    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
        try {
            auto readVec = [](const ::nicxlive::core::serde::Fghj& node, float* dst, std::size_t n) {
                std::size_t i = 0;
                for (const auto& elem : node) {
                    if (i >= n) break;
                    dst[i++] = elem.second.get_value<float>();
                }
            };

            if (auto trans = data.get_child_optional("trans")) {
                float dst[3]{translation.x, translation.y, translation.z};
                readVec(*trans, dst, 3);
                translation = Vec3{dst[0], dst[1], dst[2]};
            } else {
                translation.x = data.get<float>("trans.x", translation.x);
                translation.y = data.get<float>("trans.y", translation.y);
                translation.z = data.get<float>("trans.z", translation.z);
            }

            if (auto rot = data.get_child_optional("rot")) {
                float dst[3]{rotation.x, rotation.y, rotation.z};
                readVec(*rot, dst, 3);
                rotation = Vec3{dst[0], dst[1], dst[2]};
            } else {
                rotation.x = data.get<float>("rot.x", rotation.x);
                rotation.y = data.get<float>("rot.y", rotation.y);
                rotation.z = data.get<float>("rot.z", rotation.z);
            }

            if (auto scl = data.get_child_optional("scale")) {
                float dst[2]{scale.x, scale.y};
                readVec(*scl, dst, 2);
                scale = Vec2{dst[0], dst[1]};
            } else {
                scale.x = data.get<float>("scale.x", scale.x);
                scale.y = data.get<float>("scale.y", scale.y);
            }
            update();
        } catch (const std::exception& e) {
            return std::string(e.what());
        }
        return std::nullopt;
    }

    Transform operator*(const Transform& other) const {
        Transform tnew;
        Mat4 strs = Mat4::multiply(other.trs, trs);
        tnew.translation = strs.transformPoint(Vec3{1.0f, 1.0f, 1.0f});
        tnew.rotation = Vec3{rotation.x + other.rotation.x,
                             rotation.y + other.rotation.y,
                             rotation.z + other.rotation.z};
        tnew.scale = Vec2{scale.x * other.scale.x,
                          scale.y * other.scale.y};
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
        rotationM.m[0][0] = c;
        rotationM.m[0][1] = -s;
        rotationM.m[0][2] = 0.0f;
        rotationM.m[1][0] = s;
        rotationM.m[1][1] = c;
        rotationM.m[1][2] = 0.0f;
        rotationM.m[2][0] = 0.0f;
        rotationM.m[2][1] = 0.0f;
        rotationM.m[2][2] = 1.0f;

        Mat3x3 scaleM{};
        scaleM.m[0][0] = scale.x;
        scaleM.m[1][1] = scale.y;
        scaleM.m[2][2] = 1.0f;

        trs = multiply(translationM, multiply(rotationM, scaleM));
    }
};

} // namespace nicxlive::core::math
