#pragma once

#include <array>
#include <cctype>
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
    Normal = 0,
    Multiply = 1,
    Screen = 2,
    Overlay = 3,
    Darken = 4,
    Lighten = 5,
    ColorDodge = 6,
    LinearDodge = 7,
    AddGlow = 8,
    ColorBurn = 9,
    HardLight = 10,
    SoftLight = 11,
    Difference = 12,
    Exclusion = 13,
    Subtract = 14,
    Inverse = 15,
    DestinationIn = 16,
    ClipToLower = 17,
    SliceFromLower = 18,
    // Legacy alias kept for old callsites/configs.
    Additive = LinearDodge,
};

inline std::optional<BlendMode> parseBlendMode(std::string raw) {
    if (raw.empty()) return std::nullopt;
    std::string key;
    key.reserve(raw.size());
    for (char ch : raw) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }
    if (key.empty()) return std::nullopt;

    if (key == "normal") return BlendMode::Normal;
    if (key == "multiply") return BlendMode::Multiply;
    if (key == "screen") return BlendMode::Screen;
    if (key == "overlay") return BlendMode::Overlay;
    if (key == "darken") return BlendMode::Darken;
    if (key == "lighten") return BlendMode::Lighten;
    if (key == "colordodge") return BlendMode::ColorDodge;
    if (key == "lineardodge" || key == "additive") return BlendMode::LinearDodge;
    if (key == "addglow") return BlendMode::AddGlow;
    if (key == "colorburn") return BlendMode::ColorBurn;
    if (key == "hardlight") return BlendMode::HardLight;
    if (key == "softlight") return BlendMode::SoftLight;
    if (key == "difference") return BlendMode::Difference;
    if (key == "exclusion") return BlendMode::Exclusion;
    if (key == "subtract") return BlendMode::Subtract;
    if (key == "inverse") return BlendMode::Inverse;
    if (key == "destinationin") return BlendMode::DestinationIn;
    if (key == "cliptolower") return BlendMode::ClipToLower;
    if (key == "slicefromlower") return BlendMode::SliceFromLower;

    bool isDigits = true;
    for (char ch : key) {
        if (ch < '0' || ch > '9') {
            isDigits = false;
            break;
        }
    }
    if (isDigits) {
        int v = std::stoi(key);
        if (v >= 0 && v <= static_cast<int>(BlendMode::SliceFromLower)) {
            return static_cast<BlendMode>(v);
        }
    }
    return std::nullopt;
}

struct PartDrawPacket;
struct MaskDrawPacket;

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
