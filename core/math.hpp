#pragma once

#include "nodes/common.hpp"
#include "math/camera.hpp"

#include <cmath>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
namespace nicxlive::core::math {

// Actual ushort vector value storage, including D's zero default initialization.
// No existing float vector type or representation is replaced.
template<std::size_t N> struct UnsignedVector {
    using vt = std::uint16_t;
    static constexpr std::size_t dimension = N;
    std::array<vt, N> vector{};
    UnsignedVector() = default;
    explicit UnsignedVector(vt fill) { vector.fill(fill); }
    template<class... Components> requires (sizeof...(Components) == N && N > 1 &&
        (std::convertible_to<Components, vt> && ...))
    explicit UnsignedVector(Components... values) : vector{static_cast<vt>(values)...} {}
    vt& operator[](std::size_t index) { return vector[index]; }
    const vt& operator[](std::size_t index) const { return vector[index]; }
    bool operator==(const UnsignedVector&) const = default;
};
using Vec2us = UnsignedVector<2>;
using Vec3us = UnsignedVector<3>;
using Vec4us = UnsignedVector<4>;

// Open component policy for actual ported vector values, not a copied vector
// hierarchy. Additional Source vector types specialize this same public policy.
template<class Vector> struct SourceVectorAccess;

template<class T, std::size_t N> struct SourceVectorAccess<std::array<T, N>> {
    using Scalar = T;
    static constexpr std::size_t dimension = N;
    static T get(const std::array<T, N>& value, std::size_t index) { return value[index]; }
    static void set(std::array<T, N>& value, std::size_t index, T component) { value[index] = component; }
};

template<std::size_t N> struct SourceVectorAccess<UnsignedVector<N>> {
    using Scalar = std::uint16_t;
    static constexpr std::size_t dimension = N;
    static Scalar get(const UnsignedVector<N>& value, std::size_t index) { return value.vector[index]; }
    static void set(UnsignedVector<N>& value, std::size_t index, Scalar component) {
        value.vector[index] = component;
    }
};

template<> struct SourceVectorAccess<Vec2> {
    using Scalar = float;
    static constexpr std::size_t dimension = 2;
    static float get(const Vec2& value, std::size_t index) { return index ? value.y : value.x; }
    static void set(Vec2& value, std::size_t index, float component) {
        (index ? value.y : value.x) = component;
    }
};

template<> struct SourceVectorAccess<Vec3> {
    using Scalar = float;
    static constexpr std::size_t dimension = 3;
    static float get(const Vec3& value, std::size_t index) {
        return index == 0 ? value.x : index == 1 ? value.y : value.z;
    }
    static void set(Vec3& value, std::size_t index, float component) {
        (index == 0 ? value.x : index == 1 ? value.y : value.z) = component;
    }
};

template<> struct SourceVectorAccess<Vec4> {
    using Scalar = float;
    static constexpr std::size_t dimension = 4;
    static float get(const Vec4& value, std::size_t index) {
        return index == 0 ? value.x : index == 1 ? value.y : index == 2 ? value.z : value.w;
    }
    static void set(Vec4& value, std::size_t index, float component) {
        (index == 0 ? value.x : index == 1 ? value.y : index == 2 ? value.z : value.w) = component;
    }
};

template<class Vector> requires std::floating_point<typename SourceVectorAccess<Vector>::Scalar>
Vector ngDampenSource(Vector position, Vector target, double delta, double speed = 1) {
    using Access = SourceVectorAccess<Vector>;
    using Scalar = typename Access::Scalar;
    Vector difference{};
    for (std::size_t i = 0; i < Access::dimension; ++i)
        Access::set(difference, i, Access::get(position, i) - Access::get(target, i));
    // Inmath's vector scalar argument is vt, unlike the scalar float overload.
    const Scalar factor = static_cast<Scalar>(std::pow(0.001, delta * speed));
    Vector scaled{};
    for (std::size_t i = 0; i < Access::dimension; ++i)
        Access::set(scaled, i, Access::get(difference, i) * factor);
    Vector result{};
    for (std::size_t i = 0; i < Access::dimension; ++i)
        Access::set(result, i, Access::get(scaled, i) + Access::get(target, i));
    return result;
}

inline float dampen(float pos, float target, double delta, double speed = 1.0) {
    const float difference = pos - target;
    const double factor = std::pow(0.001, delta * speed);
    return static_cast<float>(difference * factor + target);
}

inline nodes::Vec2 dampen(const nodes::Vec2& pos, const nodes::Vec2& target, double delta, double speed = 1.0) {
    return ngDampenSource(pos, target, delta, speed);
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
