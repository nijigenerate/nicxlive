#include "phys.hpp"
#include "../../path_deformer.hpp"
#include "../../../common/utils.hpp"
#include "../../../timing.hpp"

#include <cmath>
#include <sstream>
#include <algorithm>

namespace nicxlive::core::nodes {

bool isFiniteMatrix(const Mat4& m) {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            if (!std::isfinite(m.a.a[r][c])) return false;
        }
    }
    return true;
}

bool guardFinite(PathDeformer* deformer, const std::string& context, float value, std::size_t index) {
    if (std::isfinite(value)) return true;
    if (deformer) deformer->reportInvalid(context, index == static_cast<std::size_t>(-1) ? 0 : index, Vec2{0, 0});
    return false;
}

bool guardFinite(PathDeformer* deformer, const std::string& context, const Vec2& value, std::size_t index) {
    if (std::isfinite(value.x) && std::isfinite(value.y)) return true;
    if (deformer) deformer->reportInvalid(context, index == static_cast<std::size_t>(-1) ? 0 : index, Vec2{0, 0});
    return false;
}

Mat4 requireFiniteMatrix(PathDeformer* deformer, const Mat4& m, const std::string& context) {
    if (isFiniteMatrix(m)) return m;
    if (deformer) deformer->reportInvalid(context, 0, Vec2{});
    return Mat4::identity();
}

std::string matrixSummary(const Mat4& m) {
    std::stringstream ss;
    ss << "[";
    for (int r = 0; r < 4; ++r) {
        ss << "[";
        for (int c = 0; c < 4; ++c) {
            ss << m.a.a[r][c];
            if (c < 3) ss << ",";
        }
        ss << "]";
        if (r < 3) ss << ",";
    }
    ss << "]";
    return ss.str();
}

namespace {
float clampAngle(float a) {
    const float pi = 3.14159265358979f;
    while (a > pi) a -= 2 * pi;
    while (a < -pi) a += 2 * pi;
    return a;
}

float deltaTime() {
    return static_cast<float>(::nicxlive::core::deltaTime());
}

float screenToPhysicsY(float y) { return -y; }
float physicsToScreenY(float y) { return -y; }

bool isCurrentDriver(PathDeformer* deformer, ConnectedPhysicsDriver* self) {
    if (!deformer || !deformer->driver) return false;
    auto* adapter = dynamic_cast<PathDeformer::ConnectedDriverAdapter*>(deformer->driver.get());
    return adapter && adapter->impl() == self;
}

// ---------- Pendulum helpers (from phys.d) ----------
static std::pair<std::vector<float>, std::vector<float>> extractAnglesAndLengths(PathDeformer* deformer, const std::vector<Vec2>& controlPoints) {
    std::vector<float> angles;
    std::vector<float> lengths;
    if (!deformer || controlPoints.size() < 2) return {angles, lengths};
    const float lengthEpsilon = 1e-6f;
    bool degenerate = false;
    for (std::size_t i = 1; i < controlPoints.size(); ++i) {
        Vec2 a = controlPoints[i - 1];
        Vec2 b = controlPoints[i];
        float dx = b.x - a.x;
        float dy = screenToPhysicsY(b.y) - screenToPhysicsY(a.y);
        float len = std::sqrt(dx * dx + dy * dy);
        float angle = std::atan2(dx, -dy);
        angles.push_back(angle);
        lengths.push_back(len);
        if (!std::isfinite(len) || len <= lengthEpsilon) {
            degenerate = true;
        }
    }
    if (degenerate && deformer) {
        deformer->reportPhysicsDegeneracy("pendulum:degenerateSegment");
    }
    return {angles, lengths};
}

static common::Vec2Array calculatePositions(const Vec2& base, const std::vector<float>& angles, const std::vector<float>& lengths) {
    common::Vec2Array positions;
    positions.push_back(base);
    float x = base.x;
    float y = base.y;
    for (std::size_t i = 0; i < angles.size(); ++i) {
        x += lengths[i] * std::sin(angles[i]);
        y -= lengths[i] * std::cos(angles[i]);
        positions.push_back(Vec2{x, y});
    }
    return positions;
}

static bool updatePendulum(PathDeformer* deformer,
                           const std::vector<float>& initialAngles,
                           std::vector<float>& angles,
                           std::vector<float>& angularVelocities,
                           const std::vector<float>& lengths,
                           float damping,
                           float restoreConstant,
                           float v1Velocity,
                           float timeStep,
                           float gravity,
                           float worldAngle,
                           float propagateScale,
                           const Vec2& externalForce) {
    if (lengths.empty()) return false;
    const float lengthEpsilon = 1e-6f;
    float externalTorque = externalForce.x * timeStep * lengths[0];
    for (std::size_t i = 0; i < angles.size(); ++i) {
        if (!std::isfinite(lengths[i]) || lengths[i] <= lengthEpsilon) {
            if (deformer) deformer->reportPhysicsDegeneracy("pendulum:zeroLength");
            return false;
        }
        float restoreTorque = -std::min(1.0f / timeStep, restoreConstant) * (angles[i] - initialAngles[i]);
        float dampingTorque = -damping * angularVelocities[i];
        float baseVelocityEffect = (lengths[i] != 0.0f) ? v1Velocity / lengths[i] * std::cos(angles[i]) : 0.0f;
        float gravitationalTorque = -gravity * std::sin(angles[i] + worldAngle);
        float angularAcceleration = restoreTorque + dampingTorque + baseVelocityEffect + gravitationalTorque + externalTorque;
        if (!std::isfinite(angularAcceleration)) {
            if (deformer) deformer->reportPhysicsDegeneracy("pendulum:torqueNaN");
            return false;
        }
        angularVelocities[i] += angularAcceleration * timeStep;
        if (!std::isfinite(angularVelocities[i])) {
            if (deformer) deformer->reportPhysicsDegeneracy("pendulum:velocityNaN");
            return false;
        }
        angles[i] += angularVelocities[i] * timeStep;
        if (!std::isfinite(angles[i])) {
            if (deformer) deformer->reportPhysicsDegeneracy("pendulum:angleNaN");
            return false;
        }
        externalTorque += (-angularAcceleration * std::sin(angles[i])) * timeStep * lengths[i] * propagateScale;
    }
    return true;
}

} // namespace

// ---------- ConnectedPendulumDriver ----------
ConnectedPendulumDriver::ConnectedPendulumDriver(PathDeformer* deformer) : deformer_(deformer) {}

void ConnectedPendulumDriver::reset() {
    externalForce_ = Vec2{0, 0};
}

void ConnectedPendulumDriver::rotate(float angle) { worldAngle_ = clampAngle(-angle); }

void ConnectedPendulumDriver::enforce(const Vec2& force) {
    if (!guardFinite(deformer_, "pendulum:enforceForce", force)) return;
    Vec2 scaled{force.x * inputScale_, force.y * inputScale_};
    if (!guardFinite(deformer_, "pendulum:enforceScaled", scaled)) return;
    externalForce_ = scaled;
}

void ConnectedPendulumDriver::updateDefaultShape(PathDeformer* deformer) {
    if (!deformer) return;
    std::vector<Vec2> control;
    if (deformer->originalCurve) {
        auto& cp = deformer->originalCurve->controlPoints(); // D: originalCurve.controlPoints
        control.resize(cp.size());
        for (std::size_t i = 0; i < cp.size(); ++i) {
            control[i] = Vec2{cp.xAt(i), cp.yAt(i)};
        }
    } else {
        control = deformer->controlPoints;
    }
    initialAngles_.clear();
    lengths_.clear();
    auto anglesAndLens = extractAnglesAndLengths(deformer, control);
    initialAngles_ = std::move(anglesAndLens.first);
    lengths_ = std::move(anglesAndLens.second);
    if (initialAngles_.size() != lengths_.size()) {
        initialAngles_.clear();
        lengths_.clear();
    }
}

void ConnectedPendulumDriver::setup(PathDeformer* deformer) {
    deformer_ = deformer;
    reset();
    if (!deformer_ || deformer_->vertices.size() < 2) return;
    updateDefaultShape(deformer_);
    if (initialAngles_.empty()) return;
    angles_ = initialAngles_;
    angularVelocities_.assign(angles_.size(), 0.0f);
    physDeformation_.resize(deformer_->vertices.size());
    for (std::size_t i = 0; i < physDeformation_.size(); ++i) {
        physDeformation_.set(i, Vec2{0.0f, 0.0f});
    }
    if (deformer_->originalCurve && deformer_->originalCurve->controlPoints().size() > 0) {
        auto& cp = deformer_->originalCurve->controlPoints();
        base_ = Vec2{cp.xAt(0), screenToPhysicsY(cp.yAt(0))};
    } else {
        base_ = Vec2{deformer_->controlPoints.empty() ? 0.0f : deformer_->controlPoints[0].x,
                     deformer_->controlPoints.empty() ? 0.0f : screenToPhysicsY(deformer_->controlPoints[0].y)};
    }
    guardFinite(deformer_, "pendulum:base", base_, 0);
}

void ConnectedPendulumDriver::update(PathDeformer* deformer, core::common::Vec2Array& outOffsets) {
    if (!isCurrentDriver(deformer, this)) return;
    if (angles_.empty() || lengths_.empty()) setup(deformer);
    if (angles_.empty() || lengths_.empty()) return;
    if (!guardFinite(deformer, "pendulum:externalForce", externalForce_)) return;
    float h = std::min(deltaTime(), 10.0f);
    while (h > 0.01f) {
        if (!updatePendulum(deformer, initialAngles_, angles_, angularVelocities_, lengths_, damping_, restoreConstant_, 0.0f, 0.01f, gravity_, worldAngle_, propagateScale_, externalForce_)) return;
        h -= 0.01f;
    }
    if (!updatePendulum(deformer, initialAngles_, angles_, angularVelocities_, lengths_, damping_, restoreConstant_, 0.0f, h, gravity_, worldAngle_, propagateScale_, externalForce_)) return;

    auto newPositions = calculatePositions(base_, angles_, lengths_);
    physDeformation_.resize(deformer->vertices.size());
    for (std::size_t i = 0; i < newPositions.size() && i < deformer->vertices.size(); ++i) {
        Vec2 physPos = newPositions[i];
        Vec2 screenPos{physPos.x, physicsToScreenY(physPos.y)};
        if (!guardFinite(deformer, "pendulum:newPosition", screenPos, i)) {
            physDeformation_.set(i, Vec2{0.0f, 0.0f});
            continue;
        }
        Vec2 basePoint{};
        bool hasBasePoint = false;
        if (deformer->originalCurve && i < deformer->originalCurve->controlPoints().size()) {
            auto& cp = deformer->originalCurve->controlPoints();
            basePoint = Vec2{cp.xAt(i), cp.yAt(i)};
            hasBasePoint = true;
        } else if (i < deformer->controlPoints.size()) {
            basePoint = deformer->controlPoints[i];
            hasBasePoint = true;
        }
        if (!hasBasePoint) {
            physDeformation_.set(i, Vec2{0.0f, 0.0f});
            continue;
        }
        Vec2 delta{screenPos.x - basePoint.x, screenPos.y - basePoint.y};
        if (!guardFinite(deformer, "pendulum:newDelta", delta, i)) {
            physDeformation_.set(i, Vec2{0.0f, 0.0f});
            continue;
        }
        physDeformation_.set(i, delta);
    }
    if (outOffsets.size() < deformer->vertices.size()) {
        std::size_t oldSize = outOffsets.size();
        outOffsets.resize(deformer->vertices.size());
        for (std::size_t i = oldSize; i < outOffsets.size(); ++i) {
            outOffsets.set(i, Vec2{0.0f, 0.0f});
        }
    }
    for (std::size_t i = 0; i < deformer->vertices.size(); ++i) {
        float dx = (i < physDeformation_.size()) ? physDeformation_.xAt(i) : 0.0f;
        float dy = (i < physDeformation_.size()) ? physDeformation_.yAt(i) : 0.0f;
        outOffsets.xAt(i) += dx;
        outOffsets.yAt(i) += dy;
    }
}

void ConnectedPendulumDriver::getState(PhysicsDriverState& out) const {
    out.type = "Pendulum";
    out.angles = angles_;
    out.angularVelocities = angularVelocities_;
    out.lengths = lengths_;
    out.base = base_;
    out.externalForce = externalForce_;
    out.damping = damping_;
    out.restore = restoreConstant_;
    out.timeStep = timeStep_;
    out.gravity = gravity_;
    out.inputScale = inputScale_;
    out.worldAngle = worldAngle_;
    out.propagateScale = propagateScale_;
}

void ConnectedPendulumDriver::setState(const PhysicsDriverState& state) {
    angles_ = state.angles;
    angularVelocities_ = state.angularVelocities;
    lengths_ = state.lengths;
    base_ = state.base;
    externalForce_ = state.externalForce;
    damping_ = state.damping;
    restoreConstant_ = state.restore;
    timeStep_ = state.timeStep;
    gravity_ = state.gravity;
    inputScale_ = state.inputScale;
    worldAngle_ = state.worldAngle;
    propagateScale_ = state.propagateScale;
}

// ---------- ConnectedSpringPendulumDriver ----------
ConnectedSpringPendulumDriver::ConnectedSpringPendulumDriver(PathDeformer* deformer) : deformer_(deformer) {}

void ConnectedSpringPendulumDriver::reset() {
    externalForce_ = Vec2{0, 0};
}

void ConnectedSpringPendulumDriver::enforce(const Vec2& force) {
    if (!guardFinite(deformer_, "springPendulum:enforceForce", force)) return;
    externalForce_ = force;
}

void ConnectedSpringPendulumDriver::rotate(float) {}

void ConnectedSpringPendulumDriver::updateDefaultShape(PathDeformer* deformer) {
    if (!deformer) return;
    std::vector<Vec2> basePoints;
    if (deformer->originalCurve) {
        auto& cp = deformer->originalCurve->controlPoints();
        basePoints.resize(cp.size());
        for (std::size_t i = 0; i < cp.size(); ++i) {
            basePoints[i] = Vec2{cp.xAt(i), cp.yAt(i)};
        }
    } else {
        basePoints = deformer->controlPoints;
    }
    initialPositions_.resize(basePoints.size());
    for (std::size_t i = 0; i < basePoints.size(); ++i) {
        Vec2 pt = basePoints[i];
        pt.y = screenToPhysicsY(pt.y);
        if (!guardFinite(deformer, "springPendulum:controlPoint", pt, i)) {
            initialPositions_.clear();
            return;
        }
        initialPositions_.set(i, pt);
    }
}

void ConnectedSpringPendulumDriver::setup(PathDeformer* deformer) {
    deformer_ = deformer;
    reset();
    if (!deformer_) return;
    updateDefaultShape(deformer_);
    positions_ = initialPositions_;
    velocities_.resize(positions_.size());
    for (std::size_t i = 0; i < velocities_.size(); ++i) {
        velocities_.set(i, Vec2{0.0f, 0.0f});
    }
    physDeformation_.resize(deformer_->vertices.size());
    for (std::size_t i = 0; i < physDeformation_.size(); ++i) {
        physDeformation_.set(i, Vec2{0.0f, 0.0f});
    }
    lengths_.resize(positions_.size() > 0 ? positions_.size() - 1 : 0);
    const float lengthEpsilon = 1e-6f;
    bool degenerate = false;
    for (std::size_t i = 0; i < lengths_.size(); ++i) {
        Vec2 a{positions_.xAt(i), positions_.yAt(i)};
        Vec2 b{positions_.xAt(i + 1), positions_.yAt(i + 1)};
        if (!guardFinite(deformer_, "springPendulum:position", a, i) ||
            !guardFinite(deformer_, "springPendulum:position", b, i + 1)) {
            degenerate = true;
            break;
        }
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float len = std::sqrt(dx * dx + dy * dy);
        lengths_[i] = len;
        if (!std::isfinite(len) || len <= lengthEpsilon) degenerate = true;
    }
    if (degenerate && deformer_) {
        deformer_->reportPhysicsDegeneracy("springPendulum:degenerateSegment");
        return;
    }
}

void ConnectedSpringPendulumDriver::update(PathDeformer* deformer, core::common::Vec2Array& outOffsets) {
    if (!isCurrentDriver(deformer, this)) return;
    if (lengths_.empty()) setup(deformer);
    if (lengths_.empty()) return;
    if (!guardFinite(deformer, "springPendulum:externalForce", externalForce_)) return;
    float h = std::min(deltaTime(), 10.0f);
    while (h > 0.01f) {
        updateSpringPendulum(positions_, velocities_, initialPositions_, lengths_, damping_, springConstant_, restorationConstant_, 0.01f);
        h -= 0.01f;
    }
    updateSpringPendulum(positions_, velocities_, initialPositions_, lengths_, damping_, springConstant_, restorationConstant_, h);

    for (std::size_t i = 0; i < positions_.size(); ++i) {
        Vec2 basePoint{};
        bool hasBasePoint = false;
        if (deformer->originalCurve && i < deformer->originalCurve->controlPoints().size()) {
            auto& cp = deformer->originalCurve->controlPoints();
            basePoint = Vec2{cp.xAt(i), cp.yAt(i)};
            hasBasePoint = true;
        } else if (i < deformer->controlPoints.size()) {
            basePoint = deformer->controlPoints[i];
            hasBasePoint = true;
        }
        if (!hasBasePoint) break;
        Vec2 pos{positions_.xAt(i), positions_.yAt(i)};
        Vec2 screenPos{pos.x, physicsToScreenY(pos.y)};
        if (!guardFinite(deformer, "springPendulum:position", screenPos, i)) {
            physDeformation_.set(i, Vec2{0.0f, 0.0f});
            continue;
        }
        Vec2 delta{screenPos.x - basePoint.x, screenPos.y - basePoint.y};
        if (!guardFinite(deformer, "springPendulum:delta", delta, i)) {
            physDeformation_.set(i, Vec2{0.0f, 0.0f});
            continue;
        }
        physDeformation_.set(i, delta);
    }
    if (outOffsets.size() < deformer->vertices.size()) {
        std::size_t oldSize = outOffsets.size();
        outOffsets.resize(deformer->vertices.size());
        for (std::size_t i = oldSize; i < outOffsets.size(); ++i) {
            outOffsets.set(i, Vec2{0.0f, 0.0f});
        }
    }
    for (std::size_t i = 0; i < deformer->vertices.size(); ++i) {
        float dx = (i < physDeformation_.size()) ? physDeformation_.xAt(i) : 0.0f;
        float dy = (i < physDeformation_.size()) ? physDeformation_.yAt(i) : 0.0f;
        outOffsets.xAt(i) += dx;
        outOffsets.yAt(i) += dy;
    }
}

void ConnectedSpringPendulumDriver::updateSpringPendulum(common::Vec2Array& positions,
                                                         common::Vec2Array& velocities,
                                                         const common::Vec2Array& initialPositions,
                                                         const std::vector<float>& lengths,
                                                         float damping,
                                                         float springConstant,
                                                         float restorationConstant,
                                                         float timeStep) {
    const float lengthEpsilon = 1e-6f;
    for (std::size_t i = 1; i < positions.size(); ++i) {
        Vec2 springForce{0, 0};
        if (i > 0) {
            Vec2 prev{positions.xAt(i - 1) + ((i == 1) ? externalForce_.x * timeStep : 0.0f),
                      positions.yAt(i - 1) + ((i == 1) ? externalForce_.y * timeStep : 0.0f)};
            Vec2 diff{positions.xAt(i) - prev.x, positions.yAt(i) - prev.y};
            if (!guardFinite(deformer_, "springPendulum:diffPrev", diff, i)) return;
            float diffLen = std::sqrt(diff.x * diff.x + diff.y * diff.y);
            if (!std::isfinite(diffLen) || diffLen <= lengthEpsilon) {
                if (deformer_) deformer_->reportPhysicsDegeneracy("springPendulum:zeroDiff");
                return;
            }
            springForce.x += -springConstant * (diff.x * (diffLen - lengths[i - 1]) / diffLen);
            springForce.y += -springConstant * (diff.y * (diffLen - lengths[i - 1]) / diffLen);
        }
        if (i < positions.size() - 1) {
            Vec2 next{positions.xAt(i + 1), positions.yAt(i + 1)};
            Vec2 cur{positions.xAt(i) + ((i == 0) ? externalForce_.x * timeStep : 0.0f),
                     positions.yAt(i) + ((i == 0) ? externalForce_.y * timeStep : 0.0f)};
            Vec2 diff{cur.x - next.x, cur.y - next.y};
            if (!guardFinite(deformer_, "springPendulum:diffNext", diff, i)) return;
            float diffLen = std::sqrt(diff.x * diff.x + diff.y * diff.y);
            if (!std::isfinite(diffLen) || diffLen <= lengthEpsilon) {
                if (deformer_) deformer_->reportPhysicsDegeneracy("springPendulum:zeroDiff");
                return;
            }
            springForce.x += -springConstant * (diff.x * (diffLen - lengths[i]) / diffLen);
            springForce.y += -springConstant * (diff.y * (diffLen - lengths[i]) / diffLen);
        }

        Vec2 restorationForce{-restorationConstant * (positions.xAt(i) - initialPositions.xAt(i)),
                              -restorationConstant * (positions.yAt(i) - initialPositions.yAt(i))};
        Vec2 dampingForce{-damping * velocities.xAt(i), -damping * velocities.yAt(i)};
        Vec2 acceleration{(springForce.x + dampingForce.x + restorationForce.x + gravityVec_.x),
                          (springForce.y + dampingForce.y + restorationForce.y + gravityVec_.y)};
        if (!guardFinite(deformer_, "springPendulum:acceleration", acceleration, i)) return;
        velocities.xAt(i) += acceleration.x * timeStep;
        velocities.yAt(i) += acceleration.y * timeStep;
        if (!guardFinite(deformer_, "springPendulum:velocity", Vec2{velocities.xAt(i), velocities.yAt(i)}, i)) return;
        positions.xAt(i) += velocities.xAt(i) * timeStep;
        positions.yAt(i) += velocities.yAt(i) * timeStep;
        if (!guardFinite(deformer_, "springPendulum:positionUpdate", Vec2{positions.xAt(i), positions.yAt(i)}, i)) return;
    }
}

void ConnectedSpringPendulumDriver::getState(PhysicsDriverState& out) const {
    out.type = "SpringPendulum";
    out.lengths = lengths_;
    out.externalForce = externalForce_;
    out.damping = damping_;
    out.restore = restorationConstant_;
    out.timeStep = timeStep_;
    out.gravityVec = gravityVec_;
    out.springConstant = springConstant_;
    out.restorationConstant = restorationConstant_;
}

void ConnectedSpringPendulumDriver::setState(const PhysicsDriverState& state) {
    lengths_ = state.lengths;
    externalForce_ = state.externalForce;
    damping_ = state.damping;
    restorationConstant_ = state.restorationConstant;
    if (restorationConstant_ == 0.0f) restorationConstant_ = state.restore;
    timeStep_ = state.timeStep;
    gravityVec_ = state.gravityVec;
    springConstant_ = state.springConstant;
}

} // namespace nicxlive::core::nodes
