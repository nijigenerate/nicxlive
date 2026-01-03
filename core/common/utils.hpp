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
        out.x[i] = a.x[i] + b.x[i];
        out.y[i] = a.y[i] + b.y[i];
    }
    return out;
}

inline Vec2Array operator-(const Vec2Array& a, const Vec2Array& b) {
    Vec2Array out;
    auto n = std::min(a.size(), b.size());
    out.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.x[i] = a.x[i] - b.x[i];
        out.y[i] = a.y[i] - b.y[i];
    }
    return out;
}

inline Vec2Array operator*(const Vec2Array& a, float s) {
    Vec2Array out;
    out.resize(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        out.x[i] = a.x[i] * s;
        out.y[i] = a.y[i] * s;
    }
    return out;
}

inline Vec2Array& operator+=(Vec2Array& a, const Vec2Array& b) {
    auto n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        a.x[i] += b.x[i];
        a.y[i] += b.y[i];
    }
    return a;
}

inline Vec2Array& operator-=(Vec2Array& a, const Vec2Array& b) {
    auto n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        a.x[i] -= b.x[i];
        a.y[i] -= b.y[i];
    }
    return a;
}

inline Vec2Array& operator*=(Vec2Array& a, float s) {
    for (std::size_t i = 0; i < a.size(); ++i) {
        a.x[i] *= s;
        a.y[i] *= s;
    }
    return a;
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
            out.x[i] = data.x[idx];
            out.y[i] = data.y[idx];
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
        dst.x[idx] += src.x[i];
        dst.y[idx] += src.y[i];
        changed = true;
    }
}

inline void transformAssign(Vec2Array& dst, const Vec2Array& src, const nodes::Mat4& mat) {
    dst.resize(src.size());
    for (std::size_t i = 0; i < src.size(); ++i) {
        nodes::Vec3 v{src.x[i], src.y[i], 0.0f};
        auto tv = mat.transformPoint(v);
        dst.x[i] = tv.x;
        dst.y[i] = tv.y;
    }
}

inline void transformAdd(Vec2Array& dst, const Vec2Array& src, const nodes::Mat4& mat) {
    if (dst.size() < src.size()) dst.resize(src.size());
    for (std::size_t i = 0; i < src.size(); ++i) {
        nodes::Vec3 v{src.x[i], src.y[i], 0.0f};
        auto tv = mat.transformPoint(v);
        dst.x[i] += tv.x;
        dst.y[i] += tv.y;
    }
}

inline void transformAdd(Vec2Array& dst, const Vec2Array& src, const nodes::Mat4& mat, std::size_t count) {
    if (dst.size() < count) dst.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        nodes::Vec3 v{src.x[i], src.y[i], 0.0f};
        auto tv = mat.transformPoint(v);
        dst.x[i] += tv.x;
        dst.y[i] += tv.y;
    }
}

inline Vec2Array makeZeroVecArray(std::size_t n) {
    return Vec2Array(n);
}

} // namespace nicxlive::core::common
