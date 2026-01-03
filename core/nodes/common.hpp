#pragma once

#include <array>
#include <cstdint>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <boost/qvm/mat.hpp>
#include <boost/qvm/mat_operations.hpp>
#include <boost/qvm/quat.hpp>
#include <boost/qvm/quat_operations.hpp>
#include <boost/qvm/quat_vec_operations.hpp>
#include <boost/qvm/vec.hpp>
#include <boost/qvm/vec_operations.hpp>

namespace nicxlive::core::nodes {
struct Vec2 {
    float x{0.0f};
    float y{0.0f};
};

struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

struct Vec4 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float w{0.0f};
};
} // namespace nicxlive::core::nodes

namespace nicxlive::core::common {
struct Vec2Array {
    std::vector<float> x{};
    std::vector<float> y{};

    Vec2Array() = default;
    explicit Vec2Array(std::size_t n) : x(n, 0.0f), y(n, 0.0f) {}
    Vec2Array(std::initializer_list<nodes::Vec2> init) {
        x.reserve(init.size());
        y.reserve(init.size());
        for (const auto& v : init) {
            push_back(v);
        }
    }

    std::size_t size() const { return x.size(); }
    bool empty() const { return x.empty(); }
    void resize(std::size_t n) {
        x.resize(n, 0.0f);
        y.resize(n, 0.0f);
    }
    void clear() {
        x.clear();
        y.clear();
    }
    void push_back(const nodes::Vec2& v) {
        x.push_back(v.x);
        y.push_back(v.y);
    }
    nodes::Vec2 at(std::size_t i) const { return nodes::Vec2{x.at(i), y.at(i)}; }
    nodes::Vec2 operator[](std::size_t i) const { return nodes::Vec2{x[i], y[i]}; }
};

} // namespace nicxlive::core::common

namespace nicxlive::core::nodes {
struct Mat4 {
    boost::qvm::mat<float, 4, 4> a{};

    float* operator[](int row) { return a.a[row]; }
    const float* operator[](int row) const { return a.a[row]; }

    static Mat4 identity() {
        Mat4 out{};
        out.a = boost::qvm::identity_mat<float, 4>();
        return out;
    }

    static Mat4 multiply(const Mat4& lhs, const Mat4& rhs) {
        Mat4 out{};
        out.a = boost::qvm::operator*(lhs.a, rhs.a);
        return out;
    }

    static Mat4 translation(const Vec3& t) {
        Mat4 out = identity();
        out.a.a[0][3] = t.x;
        out.a.a[1][3] = t.y;
        out.a.a[2][3] = t.z;
        return out;
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

struct Transform {
    Vec3 translation{0.0f, 0.0f, 0.0f};
    Vec3 rotation{0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    static Transform identity() {
        return Transform{};
    }

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

    void update() {
        // No cached state; kept for interface compatibility.
    }
};

using NodeId = uint32_t;

enum class BlendMode {
    Normal,
    Additive,
    Multiply,
    LinearDodge,
    AddGlow,
    Subtract,
    Inverse,
    DestinationIn,
    ClipToLower,
    SliceFromLower,
    Screen,
    Overlay,
    SoftLight,
    HardLight,
};

struct PartDrawPacket {
    Mat4 modelMatrix{Mat4::identity()};
    Mat4 puppetMatrix{Mat4::identity()};
    bool renderable{false};
    bool isMask{false};
    bool ignorePuppet{false};
    float opacity{1.0f};
    float emissionStrength{1.0f};
    BlendMode blendMode{BlendMode::Normal};
    bool useMultistageBlend{false};
    bool hasEmissionOrBumpmap{false};
    float maskThreshold{0.0f};
    Vec3 clampedTint{1.0f, 1.0f, 1.0f};
    Vec3 clampedScreen{0.0f, 0.0f, 0.0f};
    Vec2 origin{};
    uint32_t vertexOffset{0};
    uint32_t uvOffset{0};
    uint32_t deformOffset{0};
    uint32_t vertexAtlasStride{0};
    uint32_t uvAtlasStride{0};
    uint32_t deformAtlasStride{0};
    uint32_t vertexCount{0};
    uint32_t indexCount{0};
    uint32_t textureUUIDs[3]{};
    std::vector<uint16_t> indices{};
    std::vector<Vec2> vertices{};
    std::vector<Vec2> uvs{};
    ::nicxlive::core::common::Vec2Array deformation{};
    std::weak_ptr<class Part> node{};
};

struct Rect {
    float x{};
    float y{};
    float w{};
    float h{};
};

inline Rect makeRect(float x, float y, float w, float h) {
    return Rect{x, y, w, h};
}

// Debug line buffer placeholder
struct DebugLine {
    Vec3 a;
    Vec3 b;
    Vec4 color;
};

} // namespace nicxlive::core::nodes
