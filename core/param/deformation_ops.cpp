// Independent nijilive/core/nodes/defstack.d / math/veca.d arithmetic.
// BSD-2-Clause, SDK LICENSE. Uses the existing actual Vec2Array storage.
#include "deformation_ops.hpp"
#include "../compat/static_error.hpp"
#include "../math/simd.hpp"
#include <exception>
#include <span>

namespace nicxlive::core::deformation_source {
namespace {
template<Operation Op> float apply(float left, float right) {
    if constexpr (Op == Operation::Add) return left + right;
    if constexpr (Op == Operation::Subtract) return left - right;
    if constexpr (Op == Operation::Multiply) return left * right;
}

template<Operation Op> math::simd::FloatSimd apply(math::simd::FloatSimd left,
    math::simd::FloatSimd right) {
    if constexpr (Op == Operation::Add) return left + right;
    if constexpr (Op == Operation::Subtract) return left - right;
    if constexpr (Op == Operation::Multiply) return left * right;
}

template<Operation Op>
void arrayLane(std::span<float> destination, std::span<const float> source) {
    std::size_t i = 0;
    for (; i + math::simd::simdWidth <= destination.size(); i += math::simd::simdWidth) {
        const auto left = math::simd::ngLoadVecSource(std::span<const float>(destination), i);
        const auto right = math::simd::ngLoadVecSource(source, i);
        const auto result = apply<Op>(left, right);
        math::simd::ngStoreVecSource(destination, i, result);
    }
    for (; i < destination.size(); ++i) destination[i] = apply<Op>(destination[i], source[i]);
}

template<Operation Op> void scalarLane(std::span<float> destination, float source) {
    const auto right = math::simd::ngSplatSimdSource(source);
    std::size_t i = 0;
    for (; i + math::simd::simdWidth <= destination.size(); i += math::simd::simdWidth) {
        const auto left = math::simd::ngLoadVecSource(std::span<const float>(destination), i);
        const auto result = apply<Op>(left, right);
        math::simd::ngStoreVecSource(destination, i, result);
    }
    for (; i < destination.size(); ++i) destination[i] = apply<Op>(destination[i], source);
}

template<Operation Op>
void array(math::Vec2Array& destination, math::Vec2Array source) {
#ifndef NDEBUG
    if (destination.size() != source.size()) throw d_runtime::ngStaticError<d_runtime::AssertError>(
        d_runtime::RetainedString::fromStaticLiteral("Mismatched vector lengths"),
        d_runtime::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), 181);
#endif
    const auto length = destination.size();
    auto* x = destination.dataXMutable();
    const auto* sx = source.dataX();
    arrayLane<Op>({x, length}, {sx, source.size()});
    auto* y = destination.dataYMutable();
    const auto* sy = source.dataY();
    arrayLane<Op>({y, length}, {sy, source.size()});
}

template<Operation Op>
void vector(math::Vec2Array& destination, math::Vec2 source) {
    auto* x = destination.dataXMutable();
    const auto first = source.x;
    scalarLane<Op>({x, destination.size()}, first);
    auto* y = destination.dataYMutable();
    const auto second = source.y;
    scalarLane<Op>({y, destination.size()}, second);
}

template<Operation Op>
void scalar(math::Vec2Array& destination, float source) {
    auto* x = destination.dataXMutable();
    scalarLane<Op>({x, destination.size()}, source);
    auto* y = destination.dataYMutable();
    scalarLane<Op>({y, destination.size()}, source);
}
} // namespace

void ngApplyArray(math::Vec2Array& destination, math::Vec2Array source, Operation operation) {
    switch (operation) {
    case Operation::Add: array<Operation::Add>(destination, source); return;
    case Operation::Subtract: array<Operation::Subtract>(destination, source); return;
    case Operation::Multiply: array<Operation::Multiply>(destination, source); return;
    }
    std::terminate(); // Only the three internal template operation constants are valid.
}
void ngApplyVector(math::Vec2Array& destination, math::Vec2 source, Operation operation) {
    switch (operation) {
    case Operation::Add: vector<Operation::Add>(destination, source); return;
    case Operation::Subtract: vector<Operation::Subtract>(destination, source); return;
    case Operation::Multiply: vector<Operation::Multiply>(destination, source); return;
    }
    std::terminate();
}
void ngApplyScalar(math::Vec2Array& destination, float source, Operation operation) {
    switch (operation) {
    case Operation::Add: scalar<Operation::Add>(destination, source); return;
    case Operation::Subtract: scalar<Operation::Subtract>(destination, source); return;
    case Operation::Multiply: scalar<Operation::Multiply>(destination, source); return;
    }
    std::terminate();
}
} // namespace nicxlive::core::deformation_source
