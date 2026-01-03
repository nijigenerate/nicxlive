#include "connected_physics_driver.hpp"
#include "path_deformer.hpp"

#include <algorithm>
#include <cmath>

namespace nicxlive::core::nodes {

namespace {
float clampAngle(float a) {
    const float pi = 3.14159265358979f;
    while (a > pi) a -= 2 * pi;
    while (a < -pi) a += 2 * pi;
    return a;
}

float deltaTime() {
    return 0.016f; // simple stub
}
} // namespace

ConnectedPendulumDriver::ConnectedPendulumDriver(PathDeformer* deformer) : deformer_(deformer) {}

void ConnectedPendulumDriver::setup(PathDeformer* deformer) {
    deformer_ = deformer;
    reset();
    if (!deformer_) return;
    auto& verts = deformer_->vertices;
    if (verts.size() < 2) return;
    lengths_.resize(verts.size());
    angles_.resize(verts.size());
    angularVelocities_.assign(verts.size(), 0.0f);
    for (std::size_t i = 1; i < verts.size(); ++i) {
        float dx = verts[i].x - verts[i - 1].x;
        float dy = verts[i].y - verts[i - 1].y;
        lengths_[i] = std::sqrt(dx * dx + dy * dy);
        angles_[i] = std::atan2(dy, dx);
    }
    base_ = verts.empty() ? Vec2{} : verts[0];
}

void ConnectedPendulumDriver::reset() {
    angles_.clear();
    angularVelocities_.clear();
    lengths_.clear();
    externalForce_ = Vec2{0, 0};
}

void ConnectedPendulumDriver::enforce(const Vec2& force) {
    externalForce_ = Vec2{force.x * inputScale_, force.y * inputScale_};
}

void ConnectedPendulumDriver::rotate(float angle) {
    worldAngle_ = clampAngle(-angle);
}

void ConnectedPendulumDriver::updateDefaultShape(PathDeformer* deformer) {
    if (!deformer) return;
    setup(deformer);
}

void ConnectedPendulumDriver::update(PathDeformer* deformer, core::common::Vec2Array& outOffsets) {
    if (!deformer) return;
    if (lengths_.empty()) setup(deformer);
    if (lengths_.empty()) return;
    float h = std::min(deltaTime(), 10.0f);
    while (h > timeStep_) {
        for (std::size_t i = 1; i < angles_.size(); ++i) {
            angularVelocities_[i] += (-gravity_ * std::sin(angles_[i]) - damping_ * angularVelocities_[i]) * timeStep_;
            angles_[i] += angularVelocities_[i] * timeStep_;
        }
        h -= timeStep_;
    }
    if (h > 0) {
        for (std::size_t i = 1; i < angles_.size(); ++i) {
            angularVelocities_[i] += (-gravity_ * std::sin(angles_[i]) - damping_ * angularVelocities_[i]) * h;
            angles_[i] += angularVelocities_[i] * h;
        }
    }
    outOffsets.resize(deformer->vertices.size());
    Vec2 cur = base_;
    outOffsets.x[0] = 0;
    outOffsets.y[0] = 0;
    for (std::size_t i = 1; i < deformer->vertices.size(); ++i) {
        float ang = angles_[i] + worldAngle_;
        cur.x += std::cos(ang) * lengths_[i];
        cur.y += std::sin(ang) * lengths_[i];
        outOffsets.x[i] = cur.x - deformer->vertices[i].x;
        outOffsets.y[i] = cur.y - deformer->vertices[i].y;
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
    out.restore = restore_;
    out.timeStep = timeStep_;
    out.gravity = gravity_;
    out.inputScale = inputScale_;
    out.worldAngle = worldAngle_;
}

void ConnectedPendulumDriver::setState(const PhysicsDriverState& state) {
    angles_ = state.angles;
    angularVelocities_ = state.angularVelocities;
    lengths_ = state.lengths;
    base_ = state.base;
    externalForce_ = state.externalForce;
    damping_ = state.damping;
    restore_ = state.restore;
    timeStep_ = state.timeStep;
    gravity_ = state.gravity;
    inputScale_ = state.inputScale;
    worldAngle_ = state.worldAngle;
}

ConnectedSpringPendulumDriver::ConnectedSpringPendulumDriver(PathDeformer* deformer) : deformer_(deformer) {}

void ConnectedSpringPendulumDriver::setup(PathDeformer* deformer) {
    deformer_ = deformer;
    reset();
    if (!deformer_) return;
    auto& verts = deformer_->vertices;
    if (verts.size() < 2) return;
    lengths_.resize(verts.size());
    angles_.resize(verts.size());
    angularVelocities_.assign(verts.size(), 0.0f);
    for (std::size_t i = 1; i < verts.size(); ++i) {
        float dx = verts[i].x - verts[i - 1].x;
        float dy = verts[i].y - verts[i - 1].y;
        lengths_[i] = std::sqrt(dx * dx + dy * dy);
        angles_[i] = std::atan2(dy, dx);
    }
    base_ = verts.empty() ? Vec2{} : verts[0];
}

void ConnectedSpringPendulumDriver::reset() {
    angles_.clear();
    angularVelocities_.clear();
    lengths_.clear();
    externalForce_ = Vec2{0, 0};
}

void ConnectedSpringPendulumDriver::enforce(const Vec2& force) { externalForce_ = Vec2{force.x * inputScale_, force.y * inputScale_}; }
void ConnectedSpringPendulumDriver::rotate(float angle) { worldAngle_ = clampAngle(-angle); }

void ConnectedSpringPendulumDriver::updateDefaultShape(PathDeformer* deformer) { setup(deformer); }

void ConnectedSpringPendulumDriver::update(PathDeformer* deformer, core::common::Vec2Array& outOffsets) {
    if (!deformer) return;
    if (lengths_.empty()) setup(deformer);
    if (lengths_.empty()) return;
    float h = std::min(deltaTime(), 10.0f);
    while (h > timeStep_) {
        for (std::size_t i = 1; i < angles_.size(); ++i) {
            angularVelocities_[i] += (-gravity_ * std::sin(angles_[i]) - damping_ * angularVelocities_[i] - restore_ * (lengths_[i] - lengths_[1])) * timeStep_;
            angles_[i] += angularVelocities_[i] * timeStep_;
        }
        h -= timeStep_;
    }
    if (h > 0) {
        for (std::size_t i = 1; i < angles_.size(); ++i) {
            angularVelocities_[i] += (-gravity_ * std::sin(angles_[i]) - damping_ * angularVelocities_[i] - restore_ * (lengths_[i] - lengths_[1])) * h;
            angles_[i] += angularVelocities_[i] * h;
        }
    }
    outOffsets.resize(deformer->vertices.size());
    Vec2 cur = base_;
    outOffsets.x[0] = 0;
    outOffsets.y[0] = 0;
    for (std::size_t i = 1; i < deformer->vertices.size(); ++i) {
        float ang = angles_[i] + worldAngle_;
        cur.x += std::cos(ang) * lengths_[i];
        cur.y += std::sin(ang) * lengths_[i];
        outOffsets.x[i] = cur.x - deformer->vertices[i].x;
        outOffsets.y[i] = cur.y - deformer->vertices[i].y;
    }
}

void ConnectedSpringPendulumDriver::getState(PhysicsDriverState& out) const {
    out.type = "SpringPendulum";
    out.angles = angles_;
    out.angularVelocities = angularVelocities_;
    out.lengths = lengths_;
    out.base = base_;
    out.externalForce = externalForce_;
    out.damping = damping_;
    out.restore = restore_;
    out.timeStep = timeStep_;
    out.gravity = gravity_;
    out.inputScale = inputScale_;
    out.worldAngle = worldAngle_;
}

void ConnectedSpringPendulumDriver::setState(const PhysicsDriverState& state) {
    angles_ = state.angles;
    angularVelocities_ = state.angularVelocities;
    lengths_ = state.lengths;
    base_ = state.base;
    externalForce_ = state.externalForce;
    damping_ = state.damping;
    restore_ = state.restore;
    timeStep_ = state.timeStep;
    gravity_ = state.gravity;
    inputScale_ = state.inputScale;
    worldAngle_ = state.worldAngle;
}

} // namespace nicxlive::core::nodes
