#pragma once

#include "utils.hpp"
#include "shared_array.hpp"
#include "../compat/static_error.hpp"
#include <array>
#include <cstddef>

namespace nicxlive::core::common {
namespace detail {
inline void axesSourceAssertion(bool condition, unsigned line) {
    if (!condition) throw d_runtime::ngStaticError<d_runtime::AssertError>(
        "Assertion failure", "source/nijilive/math/veca_ops.d", line);
}

inline void axesSourceSlice(std::size_t requested, std::size_t length, std::size_t line) {
    if (requested > length) throw d_runtime::ngStaticError<d_runtime::ArraySliceError>(
        0, requested, length, d_runtime::RetainedString::fromStaticLiteral(
            "source/nijilive/math/veca_ops.d"), line);
}
} // namespace detail

// Complete Source veca_ops.d118-151 body; arguments are retained value descriptors.
inline void ngProjectVec2OntoAxes(const Vec2Array center, const Vec2Array reference,
    const Vec2Array axisA, const Vec2Array axisB, SharedArray<float> outA, SharedArray<float> outB) {
    const auto length = center.size();
    if (!length) return;
#ifndef NDEBUG
    detail::axesSourceAssertion(reference.size() == length, 127);
    detail::axesSourceAssertion(axisA.size() == length, 128);
    detail::axesSourceAssertion(axisB.size() == length, 129);
    detail::axesSourceAssertion(outA.size() >= length, 130);
    detail::axesSourceAssertion(outB.size() >= length, 131);
#endif
    detail::axesSourceSlice(length, center.size(), 132);
    detail::axesSourceSlice(length, center.size(), 133);
    detail::axesSourceSlice(length, reference.size(), 134);
    detail::axesSourceSlice(length, reference.size(), 135);
    detail::axesSourceSlice(length, axisA.size(), 136);
    detail::axesSourceSlice(length, axisA.size(), 137);
    detail::axesSourceSlice(length, axisB.size(), 138);
    detail::axesSourceSlice(length, axisB.size(), 139);
    detail::axesSourceSlice(length, outA.size(), 141);
    detail::axesSourceSlice(length, outB.size(), 142);
    std::size_t i = 0;
    for (; i + 4 <= length; i += 4) {
        std::array<float, 4> a, b;
        // Both projected vectors are calculated before either output store.
        for (std::size_t lane = 0; lane < 4; ++lane) {
            const auto j = i + lane;
            const float x = center.xAt(j) - reference.xAt(j);
            const float y = center.yAt(j) - reference.yAt(j);
            a[lane] = x * axisA.xAt(j) + y * axisA.yAt(j);
            b[lane] = x * axisB.xAt(j) + y * axisB.yAt(j);
        }
        for (std::size_t lane = 0; lane < 4; ++lane) outA[i + lane] = a[lane];
        for (std::size_t lane = 0; lane < 4; ++lane) outB[i + lane] = b[lane];
    }
    for (; i < length; ++i) {
        const float x = center.xAt(i) - reference.xAt(i);
        const float y = center.yAt(i) - reference.yAt(i);
        outA[i] = x * axisA.xAt(i) + y * axisA.yAt(i);
        // The scalar B reads follow A's store, as in the actual helper.
        outB[i] = x * axisB.xAt(i) + y * axisB.yAt(i);
    }
}

// Complete Source veca_ops.d153-180 body, including whole X pass before Y.
inline void ngComposeVec2FromAxes(Vec2Array& destination, const Vec2Array base,
    const SharedArray<float> scaleA, const Vec2Array directionA,
    const SharedArray<float> scaleB, const Vec2Array directionB) {
    const auto length = base.size();
    if (!length) { destination.resize(0); return; }
    destination.resize(length);
#ifndef NDEBUG
    detail::axesSourceAssertion(directionA.size() == length, 166);
    detail::axesSourceAssertion(directionB.size() == length, 167);
    detail::axesSourceAssertion(scaleA.size() >= length, 168);
    detail::axesSourceAssertion(scaleB.size() >= length, 169);
#endif
    detail::axesSourceSlice(length, destination.size(), 170);
    detail::axesSourceSlice(length, destination.size(), 171);
    detail::axesSourceSlice(length, base.size(), 172);
    detail::axesSourceSlice(length, base.size(), 173);
    detail::axesSourceSlice(length, directionA.size(), 174);
    detail::axesSourceSlice(length, directionA.size(), 175);
    detail::axesSourceSlice(length, directionB.size(), 176);
    detail::axesSourceSlice(length, directionB.size(), 177);
    const auto blend = [&](bool y, std::size_t line) {
        detail::axesSourceSlice(length, scaleA.size(), line);
        detail::axesSourceSlice(length, scaleB.size(), line);
        const auto read = [y](const Vec2Array& values, std::size_t i) {
            return y ? values.yAt(i) : values.xAt(i);
        };
        const auto write = [y, &destination](std::size_t i, float value) {
            if (y) destination.yAt(i) = value;
            else destination.xAt(i) = value;
        };
        std::size_t i = 0;
        for (; i + 4 <= length; i += 4) {
            std::array<float, 4> values;
            for (std::size_t lane = 0; lane < 4; ++lane) {
                const auto j = i + lane;
                const float initial = read(base, j);
                const float a = scaleA[j] * read(directionA, j);
                const float b = scaleB[j] * read(directionB, j);
                values[lane] = initial + a + b;
            }
            for (std::size_t lane = 0; lane < 4; ++lane) write(i + lane, values[lane]);
        }
        for (; i < length; ++i) write(i,
            read(base, i) + scaleA[i] * read(directionA, i) + scaleB[i] * read(directionB, i));
    };
    blend(false, 178);
    blend(true, 179);
}

// Complete Source veca_ops.d182-193 and rotateAxesSimd, preserving scalar alias order.
inline void ngRotateVec2TangentsToNormals(Vec2Array& normals, const Vec2Array tangents) {
    const auto length = tangents.size();
    normals.resize(length);
    if (!length) return;
    detail::axesSourceSlice(length, normals.size(), 188);
    detail::axesSourceSlice(length, normals.size(), 189);
    detail::axesSourceSlice(length, tangents.size(), 190);
    detail::axesSourceSlice(length, tangents.size(), 191);
    std::size_t i = 0;
    for (; i + 4 <= length; i += 4) {
        std::array<float, 4> x, y;
        for (std::size_t lane = 0; lane < 4; ++lane) x[lane] = tangents.xAt(i + lane);
        for (std::size_t lane = 0; lane < 4; ++lane) y[lane] = tangents.yAt(i + lane);
        for (std::size_t lane = 0; lane < 4; ++lane) normals.xAt(i + lane) = -y[lane];
        for (std::size_t lane = 0; lane < 4; ++lane) normals.yAt(i + lane) = x[lane];
    }
    for (; i < length; ++i) {
        normals.xAt(i) = -tangents.yAt(i);
        normals.yAt(i) = tangents.xAt(i);
    }
}
} // namespace nicxlive::core::common
