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
using math::Vec3Array;
using math::Vec4Array;
} // namespace nicxlive::core::nodes

namespace nicxlive::core::common {
using Vec2Array = ::nicxlive::core::math::Vec2Array;
using Vec3Array = ::nicxlive::core::math::Vec3Array;
using Vec4Array = ::nicxlive::core::math::Vec4Array;
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

struct PartDrawPacket;

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
