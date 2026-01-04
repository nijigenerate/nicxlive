#include "simple_physics_driver.hpp"
#include "../serde.hpp"
#include "../param/parameter.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <chrono>

namespace nicxlive::core::nodes {

SimplePhysicsDriver::SimplePhysicsDriver() = default;
SimplePhysicsDriver::SimplePhysicsDriver(uint32_t uuidVal, const std::shared_ptr<Node>& parent)
    : Driver(uuidVal, parent) {
    requirePreProcessTask();
    requirePostTask(0);
}

const std::string& SimplePhysicsDriver::typeId() const {
    static const std::string k = "SimplePhysics";
    return k;
}

void SimplePhysicsDriver::runBeginTask(core::RenderContext& ctx) {
    Driver::runBeginTask(ctx);
    offsetGravity = 1.0f;
    offsetLength = 0.0f;
    offsetFrequency = 1.0f;
    offsetAngleDamping = 0.5f;
    offsetLengthDamping = 0.5f;
    offsetOutputScale = {1.0f, 1.0f};
}

void SimplePhysicsDriver::runPreProcessTask(core::RenderContext&) {
    updateInputs();
}

void SimplePhysicsDriver::runPostTaskImpl(std::size_t, core::RenderContext&) {
    updateOutputs();
}

void SimplePhysicsDriver::serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const {
    Node::serializeSelfImpl(serializer, recursive, flags);
    if (has_flag(flags, SerializeNodeFlags::State)) {
        serializer.putKey("param");
        serializer.putValue(static_cast<std::size_t>(paramRef));
        serializer.putKey("model_type");
        serializer.putValue(static_cast<int>(modelType));
        serializer.putKey("map_mode");
        serializer.putValue(static_cast<int>(mapMode));
        serializer.putKey("gravity");
        serializer.putValue(gravity);
        serializer.putKey("length");
        serializer.putValue(length);
        serializer.putKey("frequency");
        serializer.putValue(frequency);
        serializer.putKey("angle_damping");
        serializer.putValue(angleDamping);
        serializer.putKey("length_damping");
        serializer.putValue(lengthDamping);
        serializer.putKey("output_scale_x");
        serializer.putValue(outputScale[0]);
        serializer.putKey("output_scale_y");
        serializer.putValue(outputScale[1]);
        serializer.putKey("local_only");
        serializer.putValue(localOnly);
    }
}

::nicxlive::core::serde::SerdeException SimplePhysicsDriver::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    auto err = Node::deserializeFromFghj(data);
    if (auto p = data.get_optional<std::size_t>("param")) paramRef = static_cast<uint32_t>(*p);
    if (auto mt = data.get_optional<int>("model_type")) modelType = static_cast<PhysicsModel>(*mt);
    if (auto mm = data.get_optional<int>("map_mode")) mapMode = static_cast<ParamMapMode>(*mm);
    if (auto g = data.get_optional<float>("gravity")) gravity = *g;
    if (auto l = data.get_optional<float>("length")) length = *l;
    if (auto f = data.get_optional<float>("frequency")) frequency = *f;
    if (auto ad = data.get_optional<float>("angle_damping")) angleDamping = *ad;
    if (auto ld = data.get_optional<float>("length_damping")) lengthDamping = *ld;
    if (auto osx = data.get_optional<float>("output_scale_x")) outputScale[0] = *osx;
    if (auto osy = data.get_optional<float>("output_scale_y")) outputScale[1] = *osy;
    if (auto lo = data.get_optional<bool>("local_only")) localOnly = *lo;
    reset();
    return err;
}

void SimplePhysicsDriver::updateDriver() {
    // Simple sine-based placeholder to mimic pendulum-ish motion.
    constexpr float dt = 0.016f;
    simPhase += frequency * dt;
    float theta = std::sin(simPhase) * gravity * offsetGravity;
    float len = std::max(0.0f, length + offsetLength);
    output.x = anchor.x + std::sin(theta) * len;
    output.y = anchor.y + std::cos(theta) * len;
}

void SimplePhysicsDriver::reset() {
    offsetGravity = 1.0f;
    offsetLength = 0.0f;
    offsetFrequency = 1.0f;
    offsetAngleDamping = 0.5f;
    offsetLengthDamping = 0.5f;
    offsetOutputScale = {1.0f, 1.0f};
    prevAnchor = {0, 0};
    prevAnchorSet = false;
    simPhase = 0.0f;
    output = {0, 0};
}

void SimplePhysicsDriver::updateInputs() {
    auto mat = transform().toMat4();
    Vec3 pos = localOnly ? Vec3{localTransform.translation.x, localTransform.translation.y, 1.0f}
                         : mat.transformPoint(Vec3{0, 0, 1});
    prevAnchor = anchor;
    anchor = Vec2{pos.x, pos.y};
}

void SimplePhysicsDriver::updateOutputs() {
    auto paramPtr = paramCached.lock();
    if (!paramPtr && paramRef != 0) {
        // no registry to resolve by UUID; leave unchanged
        return;
    }
    if (!paramPtr) return;
    Vec2 disp{output.x - anchor.x, output.y - anchor.y};
    Vec2 scaled{disp.x * outputScale[0] * offsetOutputScale[0],
                disp.y * outputScale[1] * offsetOutputScale[1]};
    if (!std::isfinite(scaled.x) || !std::isfinite(scaled.y)) {
        logPhysicsState("updateOutputs:nonFinite");
        return;
    }
    paramPtr->value = scaled;
    paramPtr->latestInternal = scaled;
}

void SimplePhysicsDriver::logPhysicsState(const std::string& context, const std::string& extra) {
    std::cerr << "[SimplePhysicsDriver] " << context;
    if (!extra.empty()) std::cerr << " " << extra;
    std::cerr << std::endl;
}

} // namespace nicxlive::core::nodes
