#pragma once

#include "../common/utils.hpp"
#include "../nodes/common.hpp"
#include "../nodes/node.hpp"
#include "binding.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nicxlive::core::param {

using common::InterpolateMode;
using common::Vec2Array;
using nodes::Node;
using nodes::Vec2;
using nodes::Vec3;
using nodes::Vec4;
using Vec2u = ::nicxlive::core::param::Vec2u;

enum class ParamMergeMode {
    Additive,
    Weighted,
    Multiplicative,
    Forced,
    Passthrough,
};

class Resource {
public:
    virtual ~Resource() = default;
};

inline float lerpValue(float a, float b, float t) {
    return a * (1.0f - t) + b * t;
}

struct DeformSlot {
    Vec2Array vertexOffsets{};
};

inline DeformSlot scaleValue(const DeformSlot& in, float scale) {
    DeformSlot out = in;
    for (std::size_t i = 0; i < out.vertexOffsets.size(); ++i) {
        out.vertexOffsets.x[i] *= scale;
        out.vertexOffsets.y[i] *= scale;
    }
    return out;
}

inline DeformSlot lerpValue(const DeformSlot& a, const DeformSlot& b, float t) {
    if (a.vertexOffsets.size() != b.vertexOffsets.size()) {
        return t < 0.5f ? a : b;
    }
    DeformSlot out;
    out.vertexOffsets = Vec2Array(a.vertexOffsets.size());
    for (std::size_t i = 0; i < a.vertexOffsets.size(); ++i) {
        out.vertexOffsets.x[i] = lerpValue(a.vertexOffsets.x[i], b.vertexOffsets.x[i], t);
        out.vertexOffsets.y[i] = lerpValue(a.vertexOffsets.y[i], b.vertexOffsets.y[i], t);
    }
    return out;
}

template <typename T>
T scaleValue(const T& in, float scale) {
    return in * scale;
}

template <>
inline float scaleValue<float>(const float& in, float scale) {
    return in * scale;
}

template <>
inline DeformSlot scaleValue<DeformSlot>(const DeformSlot& in, float scale) {
    return scaleValue(static_cast<const DeformSlot&>(in), scale);
}

inline DeformSlot operator+(const DeformSlot& a, const DeformSlot& b) {
    DeformSlot out;
    auto n = std::min(a.vertexOffsets.size(), b.vertexOffsets.size());
    out.vertexOffsets.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.vertexOffsets.x[i] = a.vertexOffsets.x[i] + b.vertexOffsets.x[i];
        out.vertexOffsets.y[i] = a.vertexOffsets.y[i] + b.vertexOffsets.y[i];
    }
    return out;
}

inline DeformSlot operator-(const DeformSlot& a, const DeformSlot& b) {
    DeformSlot out;
    auto n = std::min(a.vertexOffsets.size(), b.vertexOffsets.size());
    out.vertexOffsets.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.vertexOffsets.x[i] = a.vertexOffsets.x[i] - b.vertexOffsets.x[i];
        out.vertexOffsets.y[i] = a.vertexOffsets.y[i] - b.vertexOffsets.y[i];
    }
    return out;
}

inline DeformSlot operator*(const DeformSlot& a, float s) {
    return scaleValue(a, s);
}

class Parameter : public Resource, public std::enable_shared_from_this<Parameter> {
public:
    uint32_t uuid{0};
    std::string name{};
    std::string indexableName{};
    bool active{true};
    Vec2 value{0, 0};
    Vec2 latestInternal{0, 0};
    Vec2 previousInternal{0, 0};
    ParamMergeMode mergeMode{ParamMergeMode::Additive};
    Vec2 defaults{0, 0};
    bool isVec2{false};
    Vec2 min{0, 0};
    Vec2 max{1, 1};
    std::array<std::vector<float>, 2> axisPoints{{{0, 1}, {0, 1}}};
    std::map<std::string, std::shared_ptr<ParameterBinding>> bindingMap{};

    Parameter() = default;
    Parameter(const std::string& n, bool vec2)
        : name(n), isVec2(vec2) {
        if (!isVec2) {
            axisPoints[1] = {0};
        }
    }

    std::size_t axisPointCount(std::size_t axis) const {
        if (axis == 0) return axisPoints[0].size();
        if (!isVec2) return 1;
        return axisPoints[1].size();
    }

    float axisPointValue(std::size_t axis, std::size_t idx) const {
        const auto& arr = axisPoints[axis == 0 ? 0 : 1];
        if (idx < arr.size()) return arr[idx];
        return 0.0f;
    }

    Vec2 getKeypointOffset(const Vec2u& idx) const {
        float x = (idx.x < axisPoints[0].size()) ? axisPoints[0][idx.x] : 0.0f;
        float y = (!isVec2 || idx.y >= axisPoints[1].size()) ? 0.0f : axisPoints[1][idx.y];
        return Vec2{x, y};
    }

    void findOffset(const Vec2& offset, Vec2u& leftKeypoint, Vec2& subOffset) const {
        auto findAxis = [&](std::size_t axis, float val, std::size_t& left, float& frac) {
            const auto& arr = axisPoints[axis];
            if (arr.size() <= 1) {
                left = 0;
                frac = 0.0f;
                return;
            }
            left = 0;
            for (std::size_t i = 0; i + 1 < arr.size(); ++i) {
                if (val < arr[i + 1]) { left = i; break; }
                left = i;
            }
            std::size_t right = std::min(left + 1, arr.size() - 1);
            float denom = arr[right] - arr[left];
            frac = denom == 0.0f ? 0.0f : std::clamp((val - arr[left]) / denom, 0.0f, 1.0f);
        };

        findAxis(0, offset.x, leftKeypoint.x, subOffset.x);
        if (isVec2) {
            findAxis(1, offset.y, leftKeypoint.y, subOffset.y);
        } else {
            leftKeypoint.y = 0;
            subOffset.y = 0.0f;
        }
    }

    Vec2 normalizedValue() const {
        return Vec2{
            (value.x - min.x) / (max.x - min.x),
            (value.y - min.y) / (max.y - min.y),
        };
    }

    void setNormalizedValue(const Vec2& v) {
        value.x = v.x * (max.x - min.x) + min.x;
        value.y = v.y * (max.y - min.y) + min.y;
    }

    std::shared_ptr<ParameterBinding> getBinding(const std::shared_ptr<Node>& /*self*/, const std::string& key) const;
    std::shared_ptr<ParameterBinding> getOrAddBinding(const std::shared_ptr<Node>& target, const std::string& key);
    void removeBinding(const std::shared_ptr<ParameterBinding>& binding);
    void makeIndexable();
    void update();
};


} // namespace nicxlive::core::param

#include "binding_impl.hpp"
