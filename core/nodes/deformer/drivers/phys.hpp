#pragma once

#include "../../common.hpp"
#include "../../../common/utils.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace nicxlive::core::nodes {

class PathDeformer;

// Guard helpers translated from D phys.d
bool guardFinite(PathDeformer* deformer, const std::string& context, float value, std::size_t index = static_cast<std::size_t>(-1));
bool guardFinite(PathDeformer* deformer, const std::string& context, const Vec2& value, std::size_t index = static_cast<std::size_t>(-1));
bool isFiniteMatrix(const Mat4& m);

// Require a finite matrix; if invalid, mark and return identity
Mat4 requireFiniteMatrix(PathDeformer* deformer, const Mat4& m, const std::string& context);

// Summary string for diagnostics
std::string matrixSummary(const Mat4& m);

struct PhysicsDriverState {
    std::string type{};
    std::vector<float> angles{};
    std::vector<float> angularVelocities{};
    std::vector<float> lengths{};
    Vec2 base{};
    Vec2 externalForce{};
    float damping{1.0f};
    float restore{300.0f};
    float timeStep{0.01f};
    float gravity{9.8f};
    float inputScale{0.01f};
    float worldAngle{0.0f};
    float propagateScale{0.2f};
    // Spring driver specifics
    Vec2 gravityVec{0.0f, 9.8f};
    float springConstant{10.0f};
    float restorationConstant{0.0f};
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
    std::vector<float> initialAngles_{};
    std::vector<float> angularVelocities_{};
    std::vector<float> lengths_{};
    core::common::Vec2Array physDeformation_{};
    Vec2 base_{};
    Vec2 externalForce_{};
    float damping_{1.0f};
    float restoreConstant_{300.0f};
    float timeStep_{0.01f};
    float gravity_{9.8f};
    float inputScale_{0.01f};
    float propagateScale_{0.2f};
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
    core::common::Vec2Array positions_{};
    core::common::Vec2Array velocities_{};
    core::common::Vec2Array initialPositions_{};
    std::vector<float> lengths_{};
    core::common::Vec2Array physDeformation_{};
    Vec2 externalForce_{};
    float damping_{0.3f};
    float springConstant_{10.0f};
    float restorationConstant_{0.0f};
    float timeStep_{0.1f};
    Vec2 gravityVec_{0.0f, 9.8f};

    void updateSpringPendulum(core::common::Vec2Array& positions,
                              core::common::Vec2Array& velocities,
                              const core::common::Vec2Array& initialPositions,
                              const std::vector<float>& lengths,
                              float damping,
                              float springConstant,
                              float restorationConstant,
                              float timeStep);
};

} // namespace nicxlive::core::nodes
