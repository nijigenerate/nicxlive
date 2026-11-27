#pragma once
#include <limits>

#include "../nodes/common.hpp"
#include "../math/veca_ops.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace nicxlive::core::common {

inline Vec2Array operator+(const Vec2Array& a, const Vec2Array& b) {
    Vec2Array out;
    auto n = std::min(a.size(), b.size());
    out.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        auto av = a.at(i);
        auto bv = b.at(i);
        out.set(i, nodes::Vec2{av.x + bv.x, av.y + bv.y});
    }
    return out;
}

inline Vec2Array operator-(const Vec2Array& a, const Vec2Array& b) {
    Vec2Array out;
    auto n = std::min(a.size(), b.size());
    out.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        auto av = a.at(i);
        auto bv = b.at(i);
        out.set(i, nodes::Vec2{av.x - bv.x, av.y - bv.y});
    }
    return out;
}

inline Vec2Array operator*(const Vec2Array& a, float s) {
    Vec2Array out;
    out.resize(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        auto av = a.at(i);
        out.set(i, nodes::Vec2{av.x * s, av.y * s});
    }
    return out;
}

enum class InterpolateMode {
    Nearest = 0,
    Linear = 1,
    Step = 2,
    Stepped = Step,
    Cubic = 3,
    Bezier = 4,
    COUNT = 5,
};

inline Vec2Array gatherVec2(const Vec2Array& data, const std::vector<std::size_t>& indices) {
    Vec2Array out;
    out.resize(indices.size());
    for (std::size_t i = 0; i < indices.size(); ++i) {
        auto idx = indices[i];
        if (idx < data.size()) {
            out.set(i, data.at(idx));
        }
    }
    return out;
}

inline void scatterAddVec2(const Vec2Array& src, const std::vector<std::size_t>& indices, Vec2Array& dst, bool& changed) {
    auto count = std::min(src.size(), indices.size());
    for (std::size_t i = 0; i < count; ++i) {
        auto idx = indices[i];
        if (idx >= dst.size()) continue;
        auto dv = dst.at(idx);
        auto sv = src.at(i);
        dst.set(idx, nodes::Vec2{dv.x + sv.x, dv.y + sv.y});
        if (sv.x != 0.0f || sv.y != 0.0f) {
            changed = true;
        }
    }
}

// Existing native entry points consume the one canonical veca_ops Source body.
inline void transformAssign(Vec2Array& destination, Vec2Array source, const nodes::Mat4& matrix) {
    math::ngTransformAssignSource(destination, source, matrix);
}
inline void transformAdd(Vec2Array& destination, Vec2Array source, const nodes::Mat4& matrix,
        std::size_t count) {
    math::ngTransformAddSource(destination, source, matrix, count);
}
inline void transformAdd(Vec2Array& destination, Vec2Array source, const nodes::Mat4& matrix) {
    math::ngTransformAddSource(destination, source, matrix);
}
inline Vec2Array makeZeroVecArray(std::size_t n) {
    return Vec2Array(n);
}

} // namespace nicxlive::core::common
