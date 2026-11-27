#pragma once

#include "../nodes/common.hpp"
#include "../serde_json.hpp"
#include "../serde_reader.hpp"
#include "../math/serialization.hpp"
#include "../compat/retained_string.hpp"
#include <array>
#include <limits>

namespace nicxlive::core {
struct VerletNode {
    float distance = 1.0f;
    nodes::Vec2 position{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    nodes::Vec2 oldPosition{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};

    VerletNode() = default;
    explicit VerletNode(nodes::Vec2 pos) : position(pos), oldPosition(pos) {}

    template<class S> void serialize(S& serializer) {
        auto state = serializer.structBegin();
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("distance"));
        serializer.putValue(distance);
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("position"));
        {
            auto value = position;
            auto vectorState = serializer.listBegin();
            serializer.elemBegin();
            serializer.serializeValue(value.x);
            serializer.elemBegin();
            serializer.serializeValue(value.y);
            serializer.listEnd(vectorState);
        }
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("old_position"));
        {
            auto value = oldPosition;
            auto vectorState = serializer.listBegin();
            serializer.elemBegin();
            serializer.serializeValue(value.x);
            serializer.elemBegin();
            serializer.serializeValue(value.y);
            serializer.listEnd(vectorState);
        }
        serializer.structEnd(state);
    }
    void serialize(serde::TypedJsonWriter& serializer) const {
        const auto state = serializer.structBegin();
        serializer.putKey("distance"); serializer.putValue(distance);
        serializer.putKey("position");
        serializer.serializeValue(std::array<float, 2>{position.x, position.y});
        serializer.putKey("old_position");
        serializer.serializeValue(std::array<float, 2>{oldPosition.x, oldPosition.y});
        serializer.structEnd(state);
    }
    serde::ReadError deserializeFromFghj(serde::TypedJsonView data) {
        (void)serde::deserializeValue(data.findFirst("distance"), distance);
        readVector(data.findFirst("position"), position);
        readVector(data.findFirst("old_position"), oldPosition);
        return {};
    }
private:
    static void readVector(serde::TypedJsonView data, nodes::Vec2& value) {
        // Consume the actual shared Source reader over the original scalar cells.
        (void)math::deserialize(value, data);
    }
};
inline serde::ReadError deserializeValue(serde::TypedJsonView data, VerletNode& value) {
    return value.deserializeFromFghj(data);
}
} // namespace nicxlive::core
