// Independent nijilive/math/veca_ops.d. BSD-2-Clause, SDK LICENSE.
#include "veca_ops.hpp"
#include "simd.hpp"
#include "../compat/static_error.hpp"
#include <algorithm>
#include <span>

namespace nicxlive::core::math {
namespace {
using ReadAxis = std::span<const float>;
using WriteAxis = std::span<float>;
using simd::ngLoadVecSource;
using simd::ngStoreVecSource;
using simd::ngSplatSimdSource;
using simd::simdWidth;

void require(bool condition, std::size_t line) {
#ifndef NDEBUG
    if (!condition) throw d_runtime::ngStaticError<d_runtime::AssertError>(
        d_runtime::RetainedString::fromStaticLiteral("Assertion failure"),
        d_runtime::RetainedString::fromStaticLiteral("nijilive/math/veca_ops.d"), line);
#else
    (void)condition;
    (void)line;
#endif
}

void requireSlice(std::size_t length, std::size_t available, std::size_t line) {
    if (length > available) throw d_runtime::ngStaticError<d_runtime::ArraySliceError>(
        0, length, available,
        d_runtime::RetainedString::fromStaticLiteral("nijilive/math/veca_ops.d"), line);
}

// Use the existing canonical pointer accessors on the actual descriptor. No
// new Veca view, layout, producer API or replacement lane representation.
WriteAxis axis(Vec2Array& values, bool y, std::size_t length, std::size_t line) {
    requireSlice(length, values.size(), line);
    return {y ? values.dataYMutable() : values.dataXMutable(), length};
}
WriteAxis slice(common::SharedArray<float>& values, std::size_t length, std::size_t line) {
    requireSlice(length, values.size(), line);
    return {values.data(), length};
}

template<bool Accumulate>
void linearCombination(WriteAxis destination, ReadAxis left, ReadAxis right,
    float leftCoefficient, float rightCoefficient, float bias) {
    require(destination.size() == left.size() && left.size() == right.size(), 15);
    const auto length = destination.size();
    const auto leftVector = ngSplatSimdSource(leftCoefficient);
    const auto rightVector = ngSplatSimdSource(rightCoefficient);
    const auto biasVector = ngSplatSimdSource(bias);
    std::size_t i = 0;
    for (; i + simdWidth <= length; i += simdWidth) {
        const auto l = ngLoadVecSource(left, i);
        const auto r = ngLoadVecSource(right, i);
        auto value = leftVector * l + rightVector * r + biasVector;
        if constexpr (Accumulate) value += ngLoadVecSource(ReadAxis(destination), i);
        ngStoreVecSource(destination, i, value);
    }
    for (; i < length; ++i) {
        const auto scalar = leftCoefficient * left[i] + rightCoefficient * right[i] + bias;
        if constexpr (Accumulate) destination[i] += scalar;
        else destination[i] = scalar;
    }
}

void blendAxes(WriteAxis destination, ReadAxis base, ReadAxis scaleA,
    ReadAxis directionA, ReadAxis scaleB, ReadAxis directionB) {
    require(destination.size() == base.size(), 46);
    require(base.size() == scaleA.size(), 47);
    require(scaleA.size() == directionA.size(), 48);
    require(scaleB.size() == directionB.size(), 49);
    const auto length = destination.size();
    std::size_t i = 0;
    for (; i + simdWidth <= length; i += simdWidth) {
        const auto original = ngLoadVecSource(base, i);
        const auto termA = ngLoadVecSource(scaleA, i) * ngLoadVecSource(directionA, i);
        const auto termB = ngLoadVecSource(scaleB, i) * ngLoadVecSource(directionB, i);
        ngStoreVecSource(destination, i, original + termA + termB);
    }
    for (; i < length; ++i)
        destination[i] = base[i] + scaleA[i] * directionA[i] + scaleB[i] * directionB[i];
}

void projectAxes(WriteAxis outputA, WriteAxis outputB, ReadAxis centerX, ReadAxis centerY,
    ReadAxis referenceX, ReadAxis referenceY, ReadAxis axisAX, ReadAxis axisAY,
    ReadAxis axisBX, ReadAxis axisBY) {
    require(outputA.size() == outputB.size(), 74);
    const auto length = outputA.size();
    std::size_t i = 0;
    for (; i + simdWidth <= length; i += simdWidth) {
        const auto cx = ngLoadVecSource(centerX, i);
        const auto cy = ngLoadVecSource(centerY, i);
        const auto rx = ngLoadVecSource(referenceX, i);
        const auto ry = ngLoadVecSource(referenceY, i);
        const auto dx = cx - rx;
        const auto dy = cy - ry;
        const auto resultA = dx * ngLoadVecSource(axisAX, i) + dy * ngLoadVecSource(axisAY, i);
        const auto resultB = dx * ngLoadVecSource(axisBX, i) + dy * ngLoadVecSource(axisBY, i);
        ngStoreVecSource(outputA, i, resultA);
        ngStoreVecSource(outputB, i, resultB);
    }
    for (; i < length; ++i) {
        const auto dx = centerX[i] - referenceX[i];
        const auto dy = centerY[i] - referenceY[i];
        outputA[i] = dx * axisAX[i] + dy * axisAY[i];
        outputB[i] = dx * axisBX[i] + dy * axisBY[i];
    }
}

void rotateAxes(WriteAxis destinationX, WriteAxis destinationY, ReadAxis sourceX, ReadAxis sourceY) {
    require(destinationX.size() == destinationY.size(), 102);
    require(sourceX.size() == sourceY.size(), 103);
    const auto length = destinationX.size();
    std::size_t i = 0;
    for (; i + simdWidth <= length; i += simdWidth) {
        const auto x = ngLoadVecSource(sourceX, i);
        auto y = ngLoadVecSource(sourceY, i);
        for (auto& component : y.scalars) component = -component;
        ngStoreVecSource(destinationX, i, y);
        ngStoreVecSource(destinationY, i, x);
    }
    for (; i < length; ++i) {
        destinationX[i] = -sourceY[i];
        destinationY[i] = sourceX[i];
    }
}
} // namespace

void ngProjectVec2OntoAxesSource(Vec2Array center, Vec2Array reference, Vec2Array axisA,
    Vec2Array axisB, common::SharedArray<float> outAxisA, common::SharedArray<float> outAxisB) {
    const auto length = center.size();
    if (!length) return;
    require(reference.size() == length, 127);
    require(axisA.size() == length, 128);
    require(axisB.size() == length, 129);
    require(outAxisA.size() >= length, 130);
    require(outAxisB.size() >= length, 131);
    const auto cx = axis(center, false, length, 132);
    const auto cy = axis(center, true, length, 133);
    const auto rx = axis(reference, false, length, 134);
    const auto ry = axis(reference, true, length, 135);
    const auto ax = axis(axisA, false, length, 136);
    const auto ay = axis(axisA, true, length, 137);
    const auto bx = axis(axisB, false, length, 138);
    const auto by = axis(axisB, true, length, 139);
    const auto outputA = slice(outAxisA, length, 141);
    const auto outputB = slice(outAxisB, length, 142);
    projectAxes(outputA, outputB, cx, cy, rx, ry, ax, ay, bx, by);
}

void ngComposeVec2FromAxesSource(Vec2Array& destination, Vec2Array base,
    common::SharedArray<float> axisA, Vec2Array directionA,
    common::SharedArray<float> axisB, Vec2Array directionB) {
    const auto length = base.size();
    if (!length) { destination.resize(0); return; }
    destination.resize(length);
    require(directionA.size() == length, 166);
    require(directionB.size() == length, 167);
    require(axisA.size() >= length, 168);
    require(axisB.size() >= length, 169);
    const auto dx = axis(destination, false, length, 170);
    const auto dy = axis(destination, true, length, 171);
    const auto bx = axis(base, false, length, 172);
    const auto by = axis(base, true, length, 173);
    const auto ax = axis(directionA, false, length, 174);
    const auto ay = axis(directionA, true, length, 175);
    const auto cx = axis(directionB, false, length, 176);
    const auto cy = axis(directionB, true, length, 177);
    // Resolve both scale slices again at the second authored call site.
    const auto scaleAX = slice(axisA, length, 178);
    const auto scaleBX = slice(axisB, length, 178);
    blendAxes(dx, bx, scaleAX, ax, scaleBX, cx);
    const auto scaleAY = slice(axisA, length, 179);
    const auto scaleBY = slice(axisB, length, 179);
    blendAxes(dy, by, scaleAY, ay, scaleBY, cy);
}

void ngRotateVec2TangentsToNormalsSource(Vec2Array& normals, Vec2Array tangents) {
    const auto length = tangents.size();
    normals.resize(length);
    if (!length) return;
    const auto dx = axis(normals, false, length, 188);
    const auto dy = axis(normals, true, length, 189);
    const auto sx = axis(tangents, false, length, 190);
    const auto sy = axis(tangents, true, length, 191);
    rotateAxes(dx, dy, sx, sy);
}

void ngTransformAssignSource(Vec2Array& destination, Vec2Array source, Mat4 matrix) {
    destination.resize(source.size());
    const auto length = source.size();
    if (!length) return;
    const float m00 = matrix[0][0], m01 = matrix[0][1], m03 = matrix[0][3];
    const float m10 = matrix[1][0], m11 = matrix[1][1], m13 = matrix[1][3];
    const auto sx = axis(source, false, length, 208);
    const auto sy = axis(source, true, length, 209);
    const auto dx = axis(destination, false, length, 210);
    const auto dy = axis(destination, true, length, 211);
    linearCombination<false>(dx, sx, sy, m00, m01, m03);
    linearCombination<false>(dy, sx, sy, m10, m11, m13);
}

void ngTransformAddSource(Vec2Array& destination, Vec2Array source, Mat4 matrix, std::size_t count) {
    if (destination.empty() || source.empty()) return;
    const auto length = std::min(count, std::min(destination.size(), source.size()));
    if (!length) return;
    const float m00 = matrix[0][0], m01 = matrix[0][1];
    const float m10 = matrix[1][0], m11 = matrix[1][1];
    const auto sx = axis(source, false, length, 228);
    const auto sy = axis(source, true, length, 229);
    const auto dx = axis(destination, false, length, 230);
    const auto dy = axis(destination, true, length, 231);
    linearCombination<true>(dx, sx, sy, m00, m01, 0);
    linearCombination<true>(dy, sx, sy, m10, m11, 0);
}
} // namespace nicxlive::core::math
