#pragma once

#include "package.hpp"
#include "verlet.hpp"
#include "../timing.hpp"
#include "../compat/assert_error.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace nicxlive::core {
class PhysicsAutomation : public Automation {
    static float clamp(float value, float low, float high) {
#ifndef NDEBUG
        if (low > high) throw d_runtime::ngStaticError<d_runtime::AssertError>(
            d_runtime::RetainedString::fromStaticLiteral("Lower can't be greater than upper."),
            d_runtime::RetainedString::fromStaticLiteral("std/algorithm/comparison.d"), 587);
#endif
        if (value < low) return low;
        if (value > high) return high;
        return value;
    }
protected:
    void onUpdate() override {
        if (bindings.size() > 1) {
            auto simulationBindings = bindings;
            for (std::size_t i = 0; i < simulationBindings.size(); ++i)
                simulate(i, simulationBindings[i]);
            const auto constraintCount = 4 + bindings.size() * 2;
            for (std::size_t i = 0; i < constraintCount; ++i) constrain();
            auto outputNodes = nodes;
            for (std::size_t i = 0; i < outputNodes.size(); ++i) {
                auto& node = outputNodes[i];
                node.position.x = clamp(node.position.x, bindings[i].range.x, bindings[i].range.y);
                auto bindingStorage = bindings;
                bindingStorage[i].addAxisOffset(node.position.x);
            }
        }
    }
    void serializeSelf(serde::TypedJsonWriter& serializer) override {
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("nodes")); serializer.serializeValue(nodes);
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("damping")); serializer.putValue(damping);
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("bounciness")); serializer.putValue(bounciness);
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("gravity")); serializer.putValue(gravity);
    }
    void serializeSelf(serde::TypedJsonWriter& serializer) const override {
        serializer.putKey("nodes"); serializer.serializeValue(nodes);
        serializer.putKey("damping"); serializer.putValue(damping);
        serializer.putKey("bounciness"); serializer.putValue(bounciness);
        serializer.putKey("gravity"); serializer.putValue(gravity);
    }
    void deserializeSelf(serde::TypedJsonView data) override {
        (void)serde::deserializeValue(data.findFirst("nodes"), nodes);
        (void)serde::deserializeValue(data.findFirst("damping"), damping);
        (void)serde::deserializeValue(data.findFirst("bounciness"), bounciness);
        (void)serde::deserializeValue(data.findFirst("gravity"), gravity);
    }
public:
    common::SharedArray<VerletNode> nodes;
    float damping = 0.05f;
    float bounciness = 1.0f;
    float gravity = 20.0f;
    void bind(AutomationBinding binding) override {
        Automation::bind(std::move(binding));
        nodes.push_back(VerletNode{{0, 1}});
    }
    explicit PhysicsAutomation(std::shared_ptr<Puppet> parent) : Automation(SourceAutomationConstruction{}) {
        typeId = "physics";
        constructAutomationOwned(std::move(parent));
    }
protected:
    virtual void simulate(std::size_t i, AutomationBinding&) {
        auto nodeStorage = nodes;
        auto& node = nodeStorage[i];
        const auto previous = node.position;
        const float dtSquared = static_cast<float>(deltaTime() * deltaTime());
        const float gravityX = (0.0f * dtSquared) * bounciness;
        const float gravityY = (gravity * dtSquared) * bounciness;
        node.position = {(node.position.x - node.oldPosition.x) + gravityX,
            (node.position.y - node.oldPosition.y) + gravityY};
        node.oldPosition = previous;
    }
    virtual void constrain() {
        const auto constraintEnd = nodes.size() - 1;
        for (std::size_t i = 0; i < constraintEnd; ++i) {
            auto firstStorage = nodes;
            auto& first = firstStorage[i];
            auto secondStorage = nodes;
            auto& second = secondStorage[i + 1];
            if (i == 0) {
                auto bindingStorage = bindings;
                first.position = {bindingStorage[i].getAxisValue(), 0};
            }
            const float dx = first.position.x - second.position.x;
            const float dy = first.position.y - second.position.y;
            const float xx = dx * dx;
            const float yy = dy * dy;
            long double squaredDistance = 0;
            squaredDistance += xx;
            squaredDistance += yy;
            const float distance = static_cast<float>(std::sqrt(squaredDistance));
            float diff = 0;
            if (distance > 0) diff = (second.distance - distance) / distance;
            const float scale = static_cast<float>(0.5 * diff);
            const nodes::Vec2 translation{dx * scale, dy * scale};
            first.position.x += translation.x;
            first.position.y += translation.y;
            second.position.x -= translation.x;
            second.position.y -= translation.y;
            second.position.y = clamp(second.position.y, first.position.y, std::numeric_limits<float>::max());
        }
    }
};
template<> struct AutomationTypeId<PhysicsAutomation> { static constexpr std::string_view value = "physics"; };
} // namespace nicxlive::core
