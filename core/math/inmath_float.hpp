#pragma once

#include <bit>
#include <cmath>
#include <cstdint>

namespace nicxlive::core::math::detail {

inline float quietFloatNaN(float value) {
    return std::bit_cast<float>(std::bit_cast<std::uint32_t>(value) | 0x00400000u);
}

// Preserve the current D/inmath matrix fold's observable NaN payload ordering.
// The product retains its left NaN; += retains the newly computed product NaN.
inline float inmathProduct(float lhs, float rhs) {
    if (std::isnan(lhs)) return quietFloatNaN(lhs);
    if (std::isnan(rhs)) return quietFloatNaN(rhs);
    return lhs * rhs;
}

inline float inmathAccumulate(float accumulator, float product) {
    if (std::isnan(product)) return quietFloatNaN(product);
    if (std::isnan(accumulator)) return quietFloatNaN(accumulator);
    return accumulator + product;
}

} // namespace nicxlive::core::math::detail
