#pragma once
// Independent nijilive/core/nodes/defstack.d. BSD-2-Clause, SDK LICENSE.
#include "../math/veca.hpp"
#include <concepts>
#include <type_traits>
#include <utility>

namespace nicxlive::core::nodes { struct Deformation; }
namespace nicxlive::core::param { struct DeformSlot; }

namespace nicxlive::core::deformation_source {

enum class Operation { Add, Subtract, Multiply };
template<class T> inline constexpr bool isRecord =
    std::same_as<std::remove_cvref_t<T>, nodes::Deformation>
    || std::same_as<std::remove_cvref_t<T>, param::DeformSlot>;
template<class T> concept Operand = isRecord<T>
    || std::same_as<std::remove_cvref_t<T>, math::Vec2>
    || requires(T& value) { static_cast<float>(value); };

// One actual arithmetic body for both existing representations of the Source
// value. Neither the record, its Veca producer nor a new storage type is added.
void ngApplyArray(math::Vec2Array& destination, math::Vec2Array source, Operation operation);
void ngApplyVector(math::Vec2Array& destination, math::Vec2 source, Operation operation);
void ngApplyScalar(math::Vec2Array& destination, float source, Operation operation);

template<Operation Op, class Record, Operand Other> requires isRecord<Record>
Record ngBinary(const Record& receiver, Other other) {
    Record result;
    result.vertexOffsets = receiver.vertexOffsets.dup();
    if constexpr (isRecord<Other>) {
        ngApplyArray(result.vertexOffsets, other.vertexOffsets, Op);
    } else if constexpr (std::same_as<std::remove_cvref_t<Other>, math::Vec2>) {
        ngApplyVector(result.vertexOffsets, other, Op);
    } else {
        // Source casts the scalar only after dup and the actual Veca assignment.
        const auto scalar = static_cast<float>(other);
        ngApplyScalar(result.vertexOffsets, scalar, Op);
    }
    return result;
}

template<class Record> requires isRecord<Record>
Record ngNegate(const Record& receiver) {
    Record result;
    result.vertexOffsets = receiver.vertexOffsets.dup();
    ngApplyScalar(result.vertexOffsets, -1.0f, Operation::Multiply);
    return result;
}

template<class Record, Operand Other> requires isRecord<Record>
Record ngMultiplyRight(const Record& receiver, Other other) {
    if constexpr (isRecord<Other>) {
        Record result;
        result.vertexOffsets = other.vertexOffsets.dup();
        ngApplyArray(result.vertexOffsets, receiver.vertexOffsets, Operation::Multiply);
        return result;
    } else {
        return ngBinary<Operation::Multiply>(receiver, std::move(other));
    }
}

} // namespace nicxlive::core::deformation_source
