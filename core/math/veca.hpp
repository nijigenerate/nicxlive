#pragma once

#include "types.hpp"

#include <vector>

namespace nicxlive::core::math {

// Simplified SoA Vec2 array mirroring nijilive.math.veca for 2 lanes.
struct Vec2Array {
    std::vector<float> x{};
    std::vector<float> y{};

    Vec2Array() = default;
    explicit Vec2Array(std::size_t n) : x(n, 0.0f), y(n, 0.0f) {}
    Vec2Array(std::initializer_list<Vec2> init) {
        x.reserve(init.size());
        y.reserve(init.size());
        for (const auto& v : init) push_back(v);
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
    void push_back(const Vec2& v) {
        x.push_back(v.x);
        y.push_back(v.y);
    }
    Vec2 at(std::size_t i) const { return Vec2{x.at(i), y.at(i)}; }
    Vec2 operator[](std::size_t i) const { return Vec2{x[i], y[i]}; }
};

} // namespace nicxlive::core::math
