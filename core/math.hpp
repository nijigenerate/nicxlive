#pragma once

#include "nodes/common.hpp"

#include <cmath>
namespace nicxlive::core::math {

inline float dampen(float pos, float target, double delta, double speed = 1.0) {
    return static_cast<float>((pos - target) * std::pow(0.001, delta * speed) + target);
}

inline nodes::Vec2 dampen(const nodes::Vec2& pos, const nodes::Vec2& target, double delta, double speed = 1.0) {
    nodes::Vec2 out;
    float factor = static_cast<float>(std::pow(0.001, delta * speed));
    out.x = (pos.x - target.x) * factor + target.x;
    out.y = (pos.y - target.y) * factor + target.y;
    return out;
}

inline bool contains(const nodes::Vec4& a, const nodes::Vec2& b) {
    return b.x >= a.x && b.y >= a.y && b.x <= a.x + a.z && b.y <= a.y + a.w;
}

inline bool areLineSegmentsIntersecting(const nodes::Vec2& p1,
                                        const nodes::Vec2& p2,
                                        const nodes::Vec2& p3,
                                        const nodes::Vec2& p4) {
    float epsilon = 0.00001f;
    float denominator = (p4.y - p3.y) * (p2.x - p1.x) - (p4.x - p3.x) * (p2.y - p1.y);
    if (denominator == 0.0f) return false;

    float uA = ((p4.x - p3.x) * (p1.y - p3.y) - (p4.y - p3.y) * (p1.x - p3.x)) / denominator;
    float uB = ((p2.x - p1.x) * (p1.y - p3.y) - (p2.y - p1.y) * (p1.x - p3.x)) / denominator;
    return (uA > 0 + epsilon && uA < 1 - epsilon && uB > 0 + epsilon && uB < 1 - epsilon);
}

} // namespace nicxlive::core::math
