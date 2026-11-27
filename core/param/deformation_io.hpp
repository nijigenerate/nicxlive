#pragma once
// Independent nijilive/core/nodes/defstack.d and math/serialization.d; BSD-2-Clause (LICENSE).
#include "../common/utils.hpp"
#include "../math/serialization.hpp"
#include "../serde_reader.hpp"
#include <array>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#ifndef NICXLIVE_DEFSTACK_SOURCE_FILE
#define NICXLIVE_DEFSTACK_SOURCE_FILE "nijilive/core/nodes/defstack.d"
#endif

namespace nicxlive::core::param::detail {
template<class Serializer>
void serializeDeformationOffsets(Serializer& writer, const common::Vec2Array& values) {
    auto state = writer.listBegin();
    const auto captured = values;
    for (std::size_t vertex = 0; vertex < captured.size(); ++vertex) {
        auto offset = captured.at(vertex);
        writer.elemBegin();
        math::serialize(offset, writer);
    }
    writer.listEnd(state);
}

inline serde::ReadError deserializeDeformationOffsets(serde::TypedJsonView data,
        common::Vec2Array& values) {
    values.resize(0);
    std::size_t vertex = 0;
    for (const auto element : data.byElement()) {
        std::array<float, 2> offset{std::numeric_limits<float>::quiet_NaN(),
                                    std::numeric_limits<float>::quiet_NaN()};
        try {
            bool enough = false;
            if (element.kind() == serde::JsonKind::Array) {
                std::size_t count = 0;
                for (auto components = element.byElement(); !components.empty(); components.popFront()) ++count;
                enough = count >= 2;
            }
            if (!enough)
                throw d_runtime::ThrowableException(d_runtime::RetainedString(std::string_view(
                    "Not enough components in deformation vector")),
                    d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_DEFSTACK_SOURCE_FILE), 162);
            if (auto error = math::deserialize(offset, element))
                throw d_runtime::ThrowableException(error->msg.empty()
                    ? d_runtime::RetainedString::fromStaticLiteral("Enforcement failed")
                    : d_runtime::RetainedString(error->msg.view()),
                    d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_DEFSTACK_SOURCE_FILE), 165);
        } catch (d_runtime::ThrowableException& error) {
            const auto message = error.msg;
            std::string formatted = "Deformation vertex ";
            formatted += std::to_string(vertex);
            formatted += ": ";
            if (!message.empty()) formatted.append(message.view());
            throw d_runtime::ThrowableException(d_runtime::RetainedString(std::move(formatted)),
                d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_DEFSTACK_SOURCE_FILE), 168,
                d_runtime::ngRetainCurrentThrowable(error));
        }
        values.push_back(math::Vec2{offset[0], offset[1]});
        ++vertex;
    }
    return {};
}
}
