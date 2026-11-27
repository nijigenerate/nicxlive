#pragma once
// Independent nijilive/math/simd.d Source value/load/store adaptation.
// SDK BSD-2-Clause. Native instruction/ABI profiles are separate.
#include "../common/shared_array.hpp"
#include "../compat/static_error.hpp"
#include "../compat/native_access_fault.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

namespace nicxlive::core::math::simd {
struct alignas(16) FloatSimd {
    std::array<float, 4> scalars{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    FloatSimd operator+(FloatSimd right) const {
        FloatSimd result;
        for (std::size_t i = 0; i < 4; ++i) result.scalars[i] = scalars[i] + right.scalars[i];
        return result;
    }
    FloatSimd operator-(FloatSimd right) const {
        FloatSimd result;
        for (std::size_t i = 0; i < 4; ++i) result.scalars[i] = scalars[i] - right.scalars[i];
        return result;
    }
    FloatSimd operator*(FloatSimd right) const {
        FloatSimd result;
        for (std::size_t i = 0; i < 4; ++i) result.scalars[i] = scalars[i] * right.scalars[i];
        return result;
    }
    FloatSimd& operator+=(FloatSimd right) {
        for (std::size_t i = 0; i < 4; ++i) scalars[i] += right.scalars[i];
        return *this;
    }
};
inline constexpr std::size_t simdWidth = sizeof(FloatSimd) / sizeof(float);
inline constexpr std::size_t floatSimdAlignment = alignof(FloatSimd);
inline constexpr std::size_t floatSimdMask = floatSimdAlignment - 1;

// One actual owned FloatSimd subobject; its scalar cells are the same storage.
// Both vector lvalue access and scalar access are standard C++ object accesses,
// without inactive-union reads, proxy/reference fields or another value store.
struct alignas(16) SimdRepr : FloatSimd {
    FloatSimd& vec() { return *this; }
    const FloatSimd& vec() const { return *this; }
    void vec(FloatSimd value) { std::memmove(scalars.data(), value.scalars.data(), sizeof(FloatSimd)); }
};

namespace detail {
inline std::uintptr_t sourceSimdAddress(const float* pointer, std::size_t index) {
    return reinterpret_cast<std::uintptr_t>(pointer) +
        static_cast<std::uintptr_t>(index) * sizeof(float);
}
inline void sourceSimdSlice(std::size_t index, std::size_t length, std::size_t line) {
    const auto upper = index + simdWidth;
    if (index > upper || upper > length) throw d_runtime::ngStaticError<d_runtime::ArraySliceError>(
        index, upper, length, d_runtime::RetainedString::fromStaticLiteral("source/nijilive/math/simd.d"), line);
}
}

inline FloatSimd ngSplatSimdSource(float value) {
    SimdRepr representation;
    representation.scalars.fill(value);
    return representation.vec();
}

// Borrowed spans do not escape these pure load/store bodies. Canonical array
// overloads below retain the real owner for the complete invocation.
inline FloatSimd ngLoadVecSource(std::span<const float> data, std::size_t index) {
    const auto address = detail::sourceSimdAddress(data.data(), index);
    if ((address & floatSimdMask) == 0) {
        if (!address) d_runtime::ngFailNullAccess(false);
        FloatSimd result;
        std::memmove(result.scalars.data(), reinterpret_cast<const void*>(address), sizeof(FloatSimd));
        return result;
    }
    SimdRepr representation;
    detail::sourceSimdSlice(index, data.size(), 29);
    std::memmove(representation.scalars.data(), data.data() + index, sizeof(FloatSimd));
    return representation.vec();
}

inline FloatSimd ngLoadVecSource(const common::SharedArray<float> data, std::size_t index) {
    return ngLoadVecSource(std::span<const float>(data.data(), data.size()), index);
}

inline void ngStoreVecSource(std::span<float> data, std::size_t index, FloatSimd value) {
    const auto address = detail::sourceSimdAddress(data.data(), index);
    if ((address & floatSimdMask) == 0) {
        if (!address) d_runtime::ngFailNullAccess(true);
        std::memmove(reinterpret_cast<void*>(address), value.scalars.data(), sizeof(FloatSimd));
        return;
    }
    SimdRepr representation;
    representation.vec(value);
    detail::sourceSimdSlice(index, data.size(), 43);
    std::memmove(data.data() + index, representation.scalars.data(), sizeof(FloatSimd));
}

inline void ngStoreVecSource(common::SharedArray<float> data, std::size_t index, FloatSimd value) {
    ngStoreVecSource(std::span<float>(data.data(), data.size()), index, value);
}
} // namespace nicxlive::core::math::simd
