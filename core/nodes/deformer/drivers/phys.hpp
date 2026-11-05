#pragma once

#include "../../common.hpp"
#include "../../../common/utils.hpp"
#include "../../../serde_json.hpp"
#include "../../../serde_reader.hpp"

#include <cstddef>
#include <memory>
#include <limits>
#include <string>
#include <utility>
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
    virtual void serializeSource(core::serde::TypedJsonWriter& writer) const = 0;
    virtual core::serde::ReadError deserializeSource(core::serde::TypedJsonView data) = 0;
    virtual void setupSource() = 0;
    virtual void retargetSource(std::shared_ptr<PathDeformer> deformer) = 0;
    virtual void updateDefaultShapeSource() = 0;
    virtual void updateSource() = 0;
    virtual void copyFromSource(ConnectedPhysicsDriver* source) = 0;
};

class ConnectedPendulumDriver : public ConnectedPhysicsDriver {
public:
    void setupSource() override;
    void retargetSource(std::shared_ptr<PathDeformer> deformer) override;
    void updateDefaultShapeSource() override;
    void updateSource() override;
    void copyFromSource(ConnectedPhysicsDriver* source) override;
    void serializeSource(core::serde::TypedJsonWriter& writer) const override;
    core::serde::ReadError deserializeSource(core::serde::TypedJsonView data) override;
    explicit ConnectedPendulumDriver(PathDeformer* deformer);
    explicit ConnectedPendulumDriver(std::shared_ptr<PathDeformer> deformer);
    // Inspector attributes access the live scalar without copying driver state.
    float gravity() const noexcept { return gravity_; }
    void gravity(float value) noexcept { gravity_ = value; }
    float restoreConstant() const noexcept { return restoreConstant_; }
    void restoreConstant(float value) noexcept { restoreConstant_ = value; }
    float damping() const noexcept { return damping_; }
    void damping(float value) noexcept { damping_ = value; }
    float inputScale() const noexcept { return inputScale_; }
    void inputScale(float value) noexcept { inputScale_ = value; }
    float propagateScale() const noexcept { return propagateScale_; }
    void propagateScale(float value) noexcept { propagateScale_ = value; }
    void setup(PathDeformer* deformer) override;
    void reset() override;
    void enforce(const Vec2& force) override;
    void rotate(float angle) override;
    void update(PathDeformer* deformer, core::common::Vec2Array& outOffsets) override;
    void updateDefaultShape(PathDeformer* deformer) override;
    void getState(PhysicsDriverState& out) const override;
    void setState(const PhysicsDriverState& state) override;

public:
    std::shared_ptr<PathDeformer> deformer_{};
    core::common::SharedArray<float> angles_{};
    core::common::SharedArray<float> initialAngles_{};
    core::common::SharedArray<float> angularVelocities_{};
    core::common::SharedArray<float> lengths_{};
    core::common::Vec2Array physDeformation_{};
    Vec2 base_{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    Vec2 externalForce_{};
    float damping_{1.0f};
    float restoreConstant_{300.0f};
    float timeStep_{0.01f};
    float gravity_{9.8f};
    float inputScale_{0.01f};
    float propagateScale_{0.2f};
    float worldAngle_{0.0f};

private:
    // Source owns the genuine class reference. Only unowned native entrypoints
    // retain a raw borrow; never manufacture a shared control block for it.
    PathDeformer* nativeDeformer_{nullptr};
    bool nativeBorrowedDeformer_{false};
    std::pair<std::shared_ptr<PathDeformer>, PathDeformer*> deformerReference() const noexcept {
        auto owner = deformer_;
        auto* value = owner ? owner.get() : (nativeBorrowedDeformer_ ? nativeDeformer_ : nullptr);
        return {std::move(owner), value};
    }
    std::pair<core::common::SharedArray<float>, core::common::SharedArray<float>>
        extractAnglesAndLengthsSource(core::common::Vec2Array points);
    void updatePendulumSource(core::common::SharedArray<float>& currentAngles,
        core::common::SharedArray<float>& angularVelocities, core::common::SharedArray<float> lengths,
        float damping, float restoreConstant, float baseVelocity, float timeStep);
};

class ConnectedSpringPendulumDriver : public ConnectedPhysicsDriver {
public:
    void setupSource() override;
    void retargetSource(std::shared_ptr<PathDeformer> deformer) override;
    void updateDefaultShapeSource() override;
    void updateSource() override;
    void copyFromSource(ConnectedPhysicsDriver* source) override;
    void serializeSource(core::serde::TypedJsonWriter& writer) const override;
    core::serde::ReadError deserializeSource(core::serde::TypedJsonView data) override;
    explicit ConnectedSpringPendulumDriver(PathDeformer* deformer);
    explicit ConnectedSpringPendulumDriver(std::shared_ptr<PathDeformer> deformer);
    void setup(PathDeformer* deformer) override;
    void reset() override;
    void enforce(const Vec2& force) override;
    void rotate(float angle) override;
    void update(PathDeformer* deformer, core::common::Vec2Array& outOffsets) override;
    void updateDefaultShape(PathDeformer* deformer) override;
    void getState(PhysicsDriverState& out) const override;
    void setState(const PhysicsDriverState& state) override;

public:
    std::shared_ptr<PathDeformer> deformer_{};
    core::common::Vec2Array positions_{};
    core::common::Vec2Array velocities_{};
    core::common::Vec2Array initialPositions_{};
    core::common::SharedArray<float> lengths_{};
    core::common::Vec2Array physDeformation_{};
    Vec2 externalForce_{};
    float damping_{0.3f};
    float springConstant_{10.0f};
    float restorationConstant_{0.0f};
    float timeStep_{0.1f};
    Vec2 gravityVec_{0.0f, 9.8f};

private:
    // Source owns the genuine class reference. Only unowned native entrypoints
    // retain a raw borrow; never manufacture a shared control block for it.
    PathDeformer* nativeDeformer_{nullptr};
    bool nativeBorrowedDeformer_{false};
    std::pair<std::shared_ptr<PathDeformer>, PathDeformer*> deformerReference() const noexcept {
        auto owner = deformer_;
        auto* value = owner ? owner.get() : (nativeBorrowedDeformer_ ? nativeDeformer_ : nullptr);
        return {std::move(owner), value};
    }
    void updateSpringPendulumSource(core::common::Vec2Array& positions,
        core::common::Vec2Array& velocities, core::common::Vec2Array initialPositions,
        core::common::SharedArray<float> lengths, float damping, float springConstant,
        float restorationConstant, float timeStep);
    void updateSpringPendulum(core::common::Vec2Array& positions,
                              core::common::Vec2Array& velocities,
                              const core::common::Vec2Array& initialPositions,
                              const core::common::SharedArray<float>& lengths,
                              float damping,
                              float springConstant,
                              float restorationConstant,
                              float timeStep);
};

} // namespace nicxlive::core::nodes
