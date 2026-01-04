#pragma once

#include "../nodes/common.hpp"

#include <algorithm>
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
    Nearest,
    Linear,
    Cubic,
    Step,
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
    if (dst.size() < count) dst.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        auto idx = indices[i];
        if (idx >= dst.size()) continue;
        auto dv = dst.at(idx);
        auto sv = src.at(i);
        dst.set(idx, nodes::Vec2{dv.x + sv.x, dv.y + sv.y});
        changed = true;
    }
}

inline void transformAssign(Vec2Array& dst, const Vec2Array& src, const nodes::Mat4& mat) {
    dst.resize(src.size());
    for (std::size_t i = 0; i < src.size(); ++i) {
        auto sv = src.at(i);
        nodes::Vec3 v{sv.x, sv.y, 0.0f};
        auto tv = mat.transformPoint(v);
        dst.set(i, nodes::Vec2{tv.x, tv.y});
    }
}

inline void transformAdd(Vec2Array& dst, const Vec2Array& src, const nodes::Mat4& mat) {
    if (dst.size() < src.size()) dst.resize(src.size());
    for (std::size_t i = 0; i < src.size(); ++i) {
        auto sv = src.at(i);
        nodes::Vec3 v{sv.x, sv.y, 0.0f};
        auto tv = mat.transformPoint(v);
        auto dv = dst.at(i);
        dst.set(i, nodes::Vec2{dv.x + tv.x, dv.y + tv.y});
    }
}

inline void transformAdd(Vec2Array& dst, const Vec2Array& src, const nodes::Mat4& mat, std::size_t count) {
    if (dst.size() < count) dst.resize(count);
    for (std::size_t i = 0; i < count && i < src.size(); ++i) {
        auto sv = src.at(i);
        nodes::Vec3 v{sv.x, sv.y, 0.0f};
        auto tv = mat.transformPoint(v);
        auto dv = dst.at(i);
        dst.set(i, nodes::Vec2{dv.x + tv.x, dv.y + tv.y});
    }
}

inline Vec2Array makeZeroVecArray(std::size_t n) {
    return Vec2Array(n);
}

} // namespace nicxlive::core::common
