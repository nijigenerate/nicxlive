#pragma once

#include "common.hpp"
#include "../common/utils.hpp"

#include <vector>
#include <memory>

namespace nicxlive::core::nodes {

class PathDeformer;

struct PhysicsDriverState {
    std::string type{};
    std::vector<float> angles{};
    std::vector<float> angularVelocities{};
    std::vector<float> lengths{};
    Vec2 base{};
    Vec2 externalForce{};
    float damping{0.0f};
    float restore{0.0f};
    float timeStep{0.0f};
    float gravity{0.0f};
    float inputScale{0.0f};
    float worldAngle{0.0f};
};

class ConnectedPhysicsDriver {
public:
    virtual ~ConnectedPhysicsDriver() = default;
    virtual void setup(PathDeformer* deformer) = 0;
    virtual void reset() = 0;
    virtual void enforce(const Vec2& force) = 0;
    virtual void rotate(float angle) = 0;
    virtual void update(PathDeformer* deformer, core::common::Vec2Array& outOffsets) = 0;
    virtual void updateDefaultShape(PathDeformer* deformer) = 0;
    virtual void getState(PhysicsDriverState& out) const = 0;
    virtual void setState(const PhysicsDriverState& state) = 0;
};

class ConnectedPendulumDriver : public ConnectedPhysicsDriver {
public:
    explicit ConnectedPendulumDriver(PathDeformer* deformer);
    void setup(PathDeformer* deformer) override;
    void reset() override;
    void enforce(const Vec2& force) override;
    void rotate(float angle) override;
    void update(PathDeformer* deformer, core::common::Vec2Array& outOffsets) override;
    void updateDefaultShape(PathDeformer* deformer) override;
    void getState(PhysicsDriverState& out) const override;
    void setState(const PhysicsDriverState& state) override;

private:
    PathDeformer* deformer_{nullptr};
    std::vector<float> angles_{};
    std::vector<float> angularVelocities_{};
    std::vector<float> lengths_{};
    Vec2 base_{};
    Vec2 externalForce_{};
    float damping_{1.0f};
    float restore_{300.0f};
    float timeStep_{0.01f};
    float gravity_{9.8f};
    float inputScale_{0.01f};
    float worldAngle_{0.0f};
};

class ConnectedSpringPendulumDriver : public ConnectedPhysicsDriver {
public:
    explicit ConnectedSpringPendulumDriver(PathDeformer* deformer);
    void setup(PathDeformer* deformer) override;
    void reset() override;
    void enforce(const Vec2& force) override;
    void rotate(float angle) override;
    void update(PathDeformer* deformer, core::common::Vec2Array& outOffsets) override;
    void updateDefaultShape(PathDeformer* deformer) override;
    void getState(PhysicsDriverState& out) const override;
    void setState(const PhysicsDriverState& state) override;

private:
    PathDeformer* deformer_{nullptr};
    std::vector<float> angles_{};
    std::vector<float> angularVelocities_{};
    std::vector<float> lengths_{};
    Vec2 base_{};
    Vec2 externalForce_{};
    float damping_{1.0f};
    float restore_{50.0f};
    float timeStep_{0.01f};
    float gravity_{9.8f};
    float inputScale_{0.01f};
    float worldAngle_{0.0f};
};

} // namespace nicxlive::core::nodes
