#pragma once
// nijilive/math/serialization.d, nijigenerate Project, BSD-2-Clause (LICENSE).

#include "types.hpp"
#include "mat3.hpp"
#include "mat4.hpp"
#include "deformation_recovery.hpp"
#include "../serde_reader.hpp"
#include <array>
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace nicxlive::core::math {

// Open lowering of the original isVector/vector[i] contract. The policy returns
// the actual scalar reference; a read failure preserves earlier lane writes.
template<class Vector, class = void> struct SourceSerializationVectorAccess;

template<class Vector>
struct SourceSerializationVectorAccess<Vector, std::void_t<decltype(Vector::dimension),
        decltype(std::declval<Vector&>().vector[0])>> {
    static constexpr std::size_t dimension = Vector::dimension;
    static decltype(auto) lane(Vector& value, std::size_t index) { return (value.vector[index]); }
};

template<class T, std::size_t N> struct SourceSerializationVectorAccess<std::array<T, N>> {
    static constexpr std::size_t dimension = N;
    static T& lane(std::array<T, N>& value, std::size_t index) { return value[index]; }
};

template<> struct SourceSerializationVectorAccess<Vec2> {
    static constexpr std::size_t dimension = 2;
    static float& lane(Vec2& value, std::size_t index) { return index ? value.y : value.x; }
};
template<> struct SourceSerializationVectorAccess<Vec3> {
    static constexpr std::size_t dimension = 3;
    static float& lane(Vec3& value, std::size_t index) {
        return index == 0 ? value.x : index == 1 ? value.y : value.z;
    }
};
template<> struct SourceSerializationVectorAccess<Vec4> {
    static constexpr std::size_t dimension = 4;
    static float& lane(Vec4& value, std::size_t index) {
        return index == 0 ? value.x : index == 1 ? value.y : index == 2 ? value.z : value.w;
    }
};
template<> struct SourceSerializationVectorAccess<Vec2i> {
    static constexpr std::size_t dimension = 2;
    static std::int32_t& lane(Vec2i& value, std::size_t index) { return index ? value.y : value.x; }
};
template<> struct SourceSerializationVectorAccess<Vec4i> {
    static constexpr std::size_t dimension = 4;
    static std::int32_t& lane(Vec4i& value, std::size_t index) {
        return index == 0 ? value.x : index == 1 ? value.y : index == 2 ? value.z : value.w;
    }
};

template<class Vector> concept SourceSerializableVector = requires(Vector& value) {
    SourceSerializationVectorAccess<Vector>::dimension;
    SourceSerializationVectorAccess<Vector>::lane(value, std::size_t{});
};

// The original matrix[x][y] indexes columns first. Native Mat3/Mat4 store
// rows first; the policy maps those existing cells without transposing storage.
template<class Matrix, class = void> struct SourceMatrixSerializationAccess;
template<class Matrix>
struct SourceMatrixSerializationAccess<Matrix, std::void_t<decltype(Matrix::rows),
        decltype(Matrix::cols), decltype(std::declval<Matrix&>().matrix[0][0])>> {
    static constexpr std::size_t rows = Matrix::rows;
    static constexpr std::size_t cols = Matrix::cols;
    static decltype(auto) lane(Matrix& value, std::size_t x, std::size_t y) { return (value.matrix[x][y]); }
};
template<> struct SourceMatrixSerializationAccess<Mat4> {
    static constexpr std::size_t rows = 4, cols = 4;
    static float& lane(Mat4& value, std::size_t x, std::size_t y) { return value.a.a[y][x]; }
};
template<> struct SourceMatrixSerializationAccess<Mat3x3> {
    static constexpr std::size_t rows = 3, cols = 3;
    static float& lane(Mat3x3& value, std::size_t x, std::size_t y) { return value.m[y][x]; }
};
template<class Matrix> concept SourceSerializableMatrix = requires(Matrix& value) {
    SourceMatrixSerializationAccess<Matrix>::rows;
    SourceMatrixSerializationAccess<Matrix>::cols;
    SourceMatrixSerializationAccess<Matrix>::lane(value, std::size_t{}, std::size_t{});
};

template<SourceSerializableVector Vector, class Serializer>
void serialize(Vector value, Serializer& serializer) {
    using Access = SourceSerializationVectorAccess<Vector>;
    auto state = serializer.listBegin();
    for (std::size_t i = 0; i < Access::dimension; ++i) {
        serializer.elemBegin();
        serializer.serializeValue(Access::lane(value, i));
    }
    serializer.listEnd(state);
}

template<SourceSerializableMatrix Matrix, class Serializer>
void serialize(Matrix value, Serializer& serializer) {
    using Access = SourceMatrixSerializationAccess<Matrix>;
    auto state = serializer.listBegin();
    for (std::size_t y = 0; y < Access::rows; ++y) {
        for (std::size_t x = 0; x < Access::cols; ++x) {
            serializer.elemBegin();
            serializer.serializeValue(Access::lane(value, x, y));
        }
    }
    serializer.listEnd(state);
}

template<SourceSerializableVector Vector>
serde::ReadError deserialize(Vector& value, serde::TypedJsonView data) {
    using Access = SourceSerializationVectorAccess<Vector>;
    if (data.isMissing() || data.kind() == serde::JsonKind::Null) return {};
    if (data.kind() != serde::JsonKind::Array) {
        throw d_runtime::ThrowableException(
            d_runtime::RetainedString::fromStaticLiteral("Expected a numeric vector array"),
            d_runtime::RetainedString::fromStaticLiteral("nijilive/math/serialization.d"), 36);
    }
    std::size_t i = 0;
    for (auto element : data.byElement()) {
        if (i >= Access::dimension) break;
        auto& component = Access::lane(value, i);
        serde::ReadError error;
        if constexpr (std::is_floating_point_v<std::remove_reference_t<decltype(component)>>)
            error = serde::deserializeNumber(element, component);
        else error = serde::deserializeValue(element, component);
        if (error) return error;
        ++i;
    }
    return {};
}

inline bool isEmpty(serde::TypedJsonView value) { return value.isMissing(); }

// Source value descriptors retain the original vertex/mask backing throughout
// the existing recovery body. The offset descriptor itself is passed by ref.
inline std::size_t inRecoverDeformationOffsets(Vec2Array vertices, Vec2Array& offsets,
        common::SharedArray<std::uint8_t> missing) {
    return inRecoverDeformationOffsets(vertices, offsets,
        std::span<const std::uint8_t>(missing.data(), missing.size()));
}

} // namespace nicxlive::core::math
