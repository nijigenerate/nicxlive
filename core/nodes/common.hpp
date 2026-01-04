#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../math/types.hpp"
#include "../math/mat4.hpp"
#include "../math/transform.hpp"
#include "../math/veca.hpp"

namespace nicxlive::core::nodes {
using math::Vec2;
using math::Vec3;
using math::Vec4;
using math::Mat4;
using math::Transform;
using math::Vec2Array;
} // namespace nicxlive::core::nodes

namespace nicxlive::core::common {
using Vec2Array = ::nicxlive::core::math::Vec2Array;
}

namespace nicxlive::core::nodes {
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

// Debug draw buffer accessors（Node 実装で利用）
std::vector<DebugLine>& debugDrawBuffer();
void clearDebugDrawBuffer();

} // namespace nicxlive::core::nodes
