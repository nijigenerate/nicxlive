#pragma once

#include "driver.hpp"
#include "../serde.hpp"
#include "../param/parameter.hpp"

#include <array>
#include <vector>
#include <string>
#include <memory>

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

class PhysicsSystem;

class SimplePhysicsDriver : public Driver {
public:
    ~SimplePhysicsDriver() override;
    std::string name{"SimplePhysics"};
    bool active{true};
    uint32_t paramRef{0};
    std::shared_ptr<core::param::Parameter> param_{};
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
    float offsetAngleDamping{1.0f};
    float offsetLengthDamping{1.0f};
    std::array<float, 2> offsetOutputScale{1.0f, 1.0f};
    Vec2 anchor{0.0f, 0.0f};
    Vec2 output{0.0f, 0.0f};
    Vec2 prevAnchor{0.0f, 0.0f};
    Mat4 prevTransMat{Mat4::identity()};
    bool prevAnchorSet{false};
    float simPhase{0.0f};
    std::weak_ptr<core::param::Parameter> paramCached{};
    // runtime state for physics integration
    float angle{0.0f};
    float dAngle{0.0f};
    float lengthVel{0.0f};
    std::unique_ptr<PhysicsSystem> system{};
    PhysicsModel systemModel{PhysicsModel::Pendulum};

    SimplePhysicsDriver();
    explicit SimplePhysicsDriver(uint32_t uuidVal, const std::shared_ptr<Node>& parent = nullptr);

    const std::string& typeId() const override;
    void runBeginTask(core::RenderContext& ctx) override;
    void runPreProcessTask(core::RenderContext& ctx) override;
    void runPostTaskImpl(std::size_t priority, core::RenderContext& ctx) override;
    void serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const override;
    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;
    void drawDebug() override;

    void updateDriver() override;
    void reset() override;
    std::vector<std::shared_ptr<core::param::Parameter>> getAffectedParameters() const override;
    bool affectsParameter(const std::shared_ptr<core::param::Parameter>& p) const override;

    void setParameter(const std::shared_ptr<core::param::Parameter>& p) { paramCached = p; }

    // helpers used by physics systems (D版互換のため公開)
    float getGravity() const;
    float getLength() const;
    float getFrequency() const;
    float getAngleDamping() const;
    float getLengthDamping() const;
    Vec2 getOutputScale() const;
    void logPhysicsState(const std::string& context, const std::string& extra = "");

private:
    void updateInputs();
    void updateOutputs();

    PhysicsModel model() const { return modelType; }
    void setModel(PhysicsModel m) { modelType = m; }

    ParamMapMode mapping() const { return mapMode; }
    void setMapping(ParamMapMode m) { mapMode = m; }

    std::shared_ptr<core::param::Parameter> resolveParam();
};

} // namespace nicxlive::core::nodes
