#pragma once

#include "driver.hpp"
#include "../serde.hpp"

#include <array>
#include <vector>
#include <string>

namespace nicxlive::core::nodes {

enum class PhysicsModel {
    Pendulum,
    SpringPendulum,
};

enum class ParamMapMode {
    AngleLength,
    XY,
    LengthAngle,
    YX,
};

class SimplePhysicsDriver : public Driver {
public:
    std::string name{"SimplePhysics"};
    bool active{true};
    uint32_t paramRef{0};
    PhysicsModel modelType{PhysicsModel::Pendulum};
    ParamMapMode mapMode{ParamMapMode::AngleLength};
    bool localOnly{false};
    float gravity{1.0f};
    float length{100.0f};
    float frequency{1.0f};
    float angleDamping{0.5f};
    float lengthDamping{0.5f};
    std::array<float, 2> outputScale{1.0f, 1.0f};
    std::vector<uint32_t> affectedParameters{};

    // runtime offsets
    float offsetGravity{1.0f};
    float offsetLength{0.0f};
    float offsetFrequency{1.0f};
    float offsetAngleDamping{0.5f};
    float offsetLengthDamping{0.5f};
    std::array<float, 2> offsetOutputScale{1.0f, 1.0f};

    SimplePhysicsDriver();
    explicit SimplePhysicsDriver(uint32_t uuidVal, const std::shared_ptr<Node>& parent = nullptr);

    const std::string& typeId() const override;
    void runBeginTask(core::RenderContext& ctx) override;
    void runPreProcessTask(core::RenderContext& ctx) override;
    void runPostTaskImpl(std::size_t priority, core::RenderContext& ctx) override;
    void serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const override;
    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;

    void updateDriver() override;
    void reset() override;

    PhysicsModel model() const { return modelType; }
    void setModel(PhysicsModel m) { modelType = m; }

    ParamMapMode mapping() const { return mapMode; }
    void setMapping(ParamMapMode m) { mapMode = m; }
};

} // namespace nicxlive::core::nodes
