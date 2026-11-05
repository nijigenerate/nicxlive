#include "phys.hpp"
#include "array_allocation_error.hpp"
#include "../../path_deformer.hpp"
#include "../../../common/utils.hpp"
#include "../../../timing.hpp"
#include "../../../compat/assert_error.hpp"
#include "../../../compat/array_range_error.hpp"

#include <cmath>
#include <sstream>
#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>

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
    return false;
}

bool guardFinite(PathDeformer* deformer, const std::string& context, const Vec2& value, std::size_t index) {
    if (std::isfinite(value.x) && std::isfinite(value.y)) return true;
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
constexpr auto sourcePhysicsFile = "source/nijilive/core/nodes/deformer/drivers/phys.d";

common::SharedArray<float> sourcePhysicsNewFloats(std::size_t count) {
    if (!count) return {};
    if (count > (std::numeric_limits<std::size_t>::max)() / sizeof(float))
        ngThrowPathDriverAllocationError();
    try {
        return common::SharedArray<float>(count, std::numeric_limits<float>::quiet_NaN());
    } catch (const std::bad_alloc&) {
        ngThrowPathDriverAllocationError();
    } catch (const std::length_error&) {
        ngThrowPathDriverAllocationError();
    }
}

std::int32_t sourcePhysicsNext(std::int32_t value) {
    return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(value) + std::uint32_t{1});
}

template<class T> T& sourcePhysicsAt(common::SharedArray<T>& values, std::size_t index, std::size_t line) {
    if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
        index, values.size(), d_runtime::RetainedString::fromStaticView(sourcePhysicsFile), line);
    return values[index];
}

Vec2 sourcePhysicsPoint(const common::Vec2Array& values, std::size_t index) {
#ifndef NDEBUG
    if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::AssertError>(
        "veca index out of range", "source/nijilive/math/veca.d", 139);
#endif
    if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
        index, values.size(), d_runtime::RetainedString::fromStaticLiteral("source/nijilive/math/veca.d"), 467);
    return {values.xAt(index), values.yAt(index)};
}

void sourcePhysicsStore(common::Vec2Array& values, std::size_t index, Vec2 point) {
#ifndef NDEBUG
    if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::AssertError>(
        "veca index out of range", "source/nijilive/math/veca.d", 139);
#endif
    if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
        index, values.size(), d_runtime::RetainedString::fromStaticLiteral("source/nijilive/math/veca.d"), 475);
    values.xAt(index) = point.x;
    values.yAt(index) = point.y;
}

void sourcePhysicsZero(common::Vec2Array& values) {
    for (std::size_t i = 0; i < values.size(); ++i) values.xAt(i) = 0;
    for (std::size_t i = 0; i < values.size(); ++i) values.yAt(i) = 0;
}

float sourcePhysicsLength(Vec2 vector) {
    long double squared = 0;
    squared += vector.x * vector.x;
    squared += vector.y * vector.y;
    return static_cast<float>(std::sqrt(squared));
}

common::Vec2Array sourcePhysicsPositions(Vec2 base, common::SharedArray<float> angles,
    common::SharedArray<float> lengths) {
    common::Vec2Array positions;
    positions.append(base);
    float x = base.x, y = base.y;
    for (std::int32_t cursor = 0; static_cast<std::size_t>(cursor) < angles.size(); cursor = sourcePhysicsNext(cursor)) {
        const auto i = static_cast<std::size_t>(cursor);
        x += sourcePhysicsAt(lengths, i, 288) * std::sin(sourcePhysicsAt(angles, i, 288));
        y -= sourcePhysicsAt(lengths, i, 289) * std::cos(sourcePhysicsAt(angles, i, 289));
        positions.append(Vec2{x, y});
    }
    return positions;
}

float clampAngle(float a) {
    const float pi = 3.14159265358979f;
    while (a > pi) a -= 2 * pi;
    while (a < -pi) a += 2 * pi;
    return a;
}

double deltaTime() {
    return ::nicxlive::core::deltaTime();
}

float screenToPhysicsY(float y) { return -y; }
float physicsToScreenY(float y) { return -y; }

bool isCurrentDriver(PathDeformer* deformer, ConnectedPhysicsDriver* self) {
    if (!deformer) return false;
    const auto driver = deformer->getDriverSource();
    auto* adapter = dynamic_cast<PathDeformer::ConnectedDriverAdapter*>(driver.get());
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

static common::Vec2Array calculatePositions(const Vec2& base, const common::SharedArray<float>& angles, const common::SharedArray<float>& lengths) {
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
                           const common::SharedArray<float>& initialAngles,
                           common::SharedArray<float>& angles,
                           common::SharedArray<float>& angularVelocities,
                           const common::SharedArray<float>& lengths,
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
ConnectedPendulumDriver::ConnectedPendulumDriver(PathDeformer* deformer)
    : nativeDeformer_(deformer) {
    if (deformer)
        deformer_ = std::dynamic_pointer_cast<PathDeformer>(deformer->weak_from_this().lock());
    nativeBorrowedDeformer_ = deformer && !deformer_;
}

ConnectedPendulumDriver::ConnectedPendulumDriver(std::shared_ptr<PathDeformer> deformer)
    : deformer_(deformer), nativeDeformer_(deformer.get()) {}

void ConnectedPendulumDriver::reset() {
    externalForce_ = Vec2{0, 0};
}

void ConnectedPendulumDriver::rotate(float angle) {
    if (!guardFinite(deformerReference().second, "pendulum:rotateAngle", angle)) return;
    worldAngle_ = -angle;
}

void ConnectedPendulumDriver::enforce(const Vec2& force) {
    if (!guardFinite(deformerReference().second, "pendulum:enforceForce", force)) return;
    Vec2 scaled{force.x * inputScale_, force.y * inputScale_};
    if (!guardFinite(deformerReference().second, "pendulum:enforceScaled", scaled)) return;
    externalForce_ = scaled;
}

void ConnectedPendulumDriver::updateDefaultShape(PathDeformer* deformer) {
    std::vector<Vec2> control;
    const auto& points = deformer->originalCurve->controlPoints();
    control.reserve(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Vec2 point{points.xAt(i), points.yAt(i)};
        if (!guardFinite(deformer, "pendulum:controlPoint", Vec2{point.x, screenToPhysicsY(point.y)}, i))
            return;
        control.push_back(point);
    }
    auto values = extractAnglesAndLengths(deformer, control);
    initialAngles_ = common::SharedArray<float>(std::move(values.first));
    lengths_ = common::SharedArray<float>(std::move(values.second));
    for (const float angle : initialAngles_) {
        if (!guardFinite(deformer, "pendulum:initialAngle", angle)) {
            initialAngles_.clear(); lengths_.clear(); return;
        }
    }
    for (const float length : lengths_) {
        if (!guardFinite(deformer, "pendulum:initialLength", length)) {
            initialAngles_.clear(); lengths_.clear(); return;
        }
    }
}

void ConnectedPendulumDriver::setup(PathDeformer* deformer) {
    {
        [[maybe_unused]] const auto previousOwner = deformer_;
        nativeDeformer_ = deformer;
        deformer_ = deformer
            ? std::dynamic_pointer_cast<PathDeformer>(deformer->weak_from_this().lock())
            : std::shared_ptr<PathDeformer>{};
        nativeBorrowedDeformer_ = deformer && !deformer_;
    }
    if (nativeDeformer_->getVertices().size() < 2) return;
    updateDefaultShape(nativeDeformer_);
    if (!isCurrentDriver(nativeDeformer_, this)) return;
    angles_ = initialAngles_.duplicate();
    angularVelocities_.assign(angles_.size(), 0.0f);
    physDeformation_.resize(nativeDeformer_->getVertices().size());
    physDeformation_.fill(Vec2{0.0f, 0.0f});
    const auto& points = nativeDeformer_->originalCurve->controlPoints();
    base_ = Vec2{points.xAt(0), screenToPhysicsY(points.yAt(0))};
    if (!guardFinite(deformerReference().second, "pendulum:base", base_, 0)) return;
}

void ConnectedPendulumDriver::update(PathDeformer* deformer, core::common::Vec2Array& outOffsets) {
    if (!isCurrentDriver(deformer, this)) return;
    if (angles_.empty() || lengths_.empty()) setup(deformer);
    if (angles_.empty() || lengths_.empty()) return;
    if (!guardFinite(deformer, "pendulum:externalForce", externalForce_)) return;
    float h = static_cast<float>(std::min(deltaTime(), 10.0));
    while (h > 0.01f) {
        if (!updatePendulum(deformer, initialAngles_, angles_, angularVelocities_, lengths_, damping_, restoreConstant_, 0.0f, 0.01f, gravity_, worldAngle_, propagateScale_, externalForce_)) return;
        h -= 0.01f;
    }
    if (!updatePendulum(deformer, initialAngles_, angles_, angularVelocities_, lengths_, damping_, restoreConstant_, 0.0f, h, gravity_, worldAngle_, propagateScale_, externalForce_)) return;

    auto newPositions = calculatePositions(base_, angles_, lengths_);
    physDeformation_.resize(deformer->getVertices().size());
    for (std::size_t i = 0; i < newPositions.size() && i < deformer->getVertices().size(); ++i) {
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
    if (outOffsets.size() < deformer->getVertices().size()) {
        std::size_t oldSize = outOffsets.size();
        outOffsets.resize(deformer->getVertices().size());
        for (std::size_t i = oldSize; i < outOffsets.size(); ++i) {
            outOffsets.set(i, Vec2{0.0f, 0.0f});
        }
    }
    for (std::size_t i = 0; i < deformer->getVertices().size(); ++i) {
        float dx = (i < physDeformation_.size()) ? physDeformation_.xAt(i) : 0.0f;
        float dy = (i < physDeformation_.size()) ? physDeformation_.yAt(i) : 0.0f;
        outOffsets.xAt(i) += dx;
        outOffsets.yAt(i) += dy;
    }
}

void ConnectedPendulumDriver::getState(PhysicsDriverState& out) const {
    out.type = "Pendulum";
    out.angles.clear();
    for (const auto value : angles_) out.angles.push_back(value);
    out.angularVelocities.clear();
    for (const auto value : angularVelocities_) out.angularVelocities.push_back(value);
    out.lengths.clear();
    for (const auto value : lengths_) out.lengths.push_back(value);
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
    angles_ = common::SharedArray<float>(state.angles);
    angularVelocities_ = common::SharedArray<float>(state.angularVelocities);
    lengths_ = common::SharedArray<float>(state.lengths);
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
ConnectedSpringPendulumDriver::ConnectedSpringPendulumDriver(PathDeformer* deformer)
    : nativeDeformer_(deformer) {
    if (deformer)
        deformer_ = std::dynamic_pointer_cast<PathDeformer>(deformer->weak_from_this().lock());
    nativeBorrowedDeformer_ = deformer && !deformer_;
}

ConnectedSpringPendulumDriver::ConnectedSpringPendulumDriver(std::shared_ptr<PathDeformer> deformer)
    : deformer_(deformer), nativeDeformer_(deformer.get()) {}

void ConnectedSpringPendulumDriver::reset() {
    externalForce_ = Vec2{0, 0};
}

void ConnectedSpringPendulumDriver::enforce(const Vec2& force) {
    if (!guardFinite(deformerReference().second, "springPendulum:enforceForce", force)) return;
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
    {
        [[maybe_unused]] const auto previousOwner = deformer_;
        nativeDeformer_ = deformer;
        deformer_ = deformer
            ? std::dynamic_pointer_cast<PathDeformer>(deformer->weak_from_this().lock())
            : std::shared_ptr<PathDeformer>{};
        nativeBorrowedDeformer_ = deformer && !deformer_;
    }
    updateDefaultShape(nativeDeformer_);
    if (!nativeDeformer_) return;
    positions_ = initialPositions_.dup();
    velocities_.resize(positions_.size());
    for (std::size_t i = 0; i < velocities_.size(); ++i) {
        velocities_.set(i, Vec2{0.0f, 0.0f});
    }
    physDeformation_.resize(nativeDeformer_->getVertices().size());
    for (std::size_t i = 0; i < physDeformation_.size(); ++i) {
        physDeformation_.set(i, Vec2{0.0f, 0.0f});
    }
    if (positions_.empty()) ngThrowPathDriverAllocationError();
    lengths_.resize(positions_.size() - 1);
    const float lengthEpsilon = 1e-6f;
    bool degenerate = false;
    for (std::size_t i = 0; i < lengths_.size(); ++i) {
        Vec2 a{positions_.xAt(i), positions_.yAt(i)};
        Vec2 b{positions_.xAt(i + 1), positions_.yAt(i + 1)};
        if (!guardFinite(deformerReference().second, "springPendulum:position", a, i) ||
            !guardFinite(deformerReference().second, "springPendulum:position", b, i + 1)) {
            degenerate = true;
            break;
        }
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float len = std::sqrt(dx * dx + dy * dy);
        lengths_[i] = len;
        if (!std::isfinite(len) || len <= lengthEpsilon) degenerate = true;
    }
    if (degenerate && nativeDeformer_) {
        nativeDeformer_->reportPhysicsDegeneracy("springPendulum:degenerateSegment");
        return;
    }
}

void ConnectedSpringPendulumDriver::update(PathDeformer* deformer, core::common::Vec2Array& outOffsets) {
    if (!isCurrentDriver(deformer, this)) return;
    if (lengths_.empty()) setup(deformer);
    if (lengths_.empty()) return;
    if (!guardFinite(deformer, "springPendulum:externalForce", externalForce_)) return;
    float h = static_cast<float>(std::min(deltaTime(), 10.0));
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
    if (outOffsets.size() < deformer->getVertices().size()) {
        std::size_t oldSize = outOffsets.size();
        outOffsets.resize(deformer->getVertices().size());
        for (std::size_t i = oldSize; i < outOffsets.size(); ++i) {
            outOffsets.set(i, Vec2{0.0f, 0.0f});
        }
    }
    for (std::size_t i = 0; i < deformer->getVertices().size(); ++i) {
        float dx = (i < physDeformation_.size()) ? physDeformation_.xAt(i) : 0.0f;
        float dy = (i < physDeformation_.size()) ? physDeformation_.yAt(i) : 0.0f;
        outOffsets.xAt(i) += dx;
        outOffsets.yAt(i) += dy;
    }
}

void ConnectedSpringPendulumDriver::updateSpringPendulum(common::Vec2Array& positions,
                                                         common::Vec2Array& velocities,
                                                         const common::Vec2Array& initialPositions,
                                                         const common::SharedArray<float>& lengths,
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
            if (!guardFinite(deformerReference().second, "springPendulum:diffPrev", diff, i)) return;
            float diffLen = std::sqrt(diff.x * diff.x + diff.y * diff.y);
            if (!std::isfinite(diffLen) || diffLen <= lengthEpsilon) {
                if (nativeDeformer_) nativeDeformer_->reportPhysicsDegeneracy("springPendulum:zeroDiff");
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
            if (!guardFinite(deformerReference().second, "springPendulum:diffNext", diff, i)) return;
            float diffLen = std::sqrt(diff.x * diff.x + diff.y * diff.y);
            if (!std::isfinite(diffLen) || diffLen <= lengthEpsilon) {
                if (nativeDeformer_) nativeDeformer_->reportPhysicsDegeneracy("springPendulum:zeroDiff");
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
        if (!guardFinite(deformerReference().second, "springPendulum:acceleration", acceleration, i)) return;
        velocities.xAt(i) += acceleration.x * timeStep;
        velocities.yAt(i) += acceleration.y * timeStep;
        if (!guardFinite(deformerReference().second, "springPendulum:velocity", Vec2{velocities.xAt(i), velocities.yAt(i)}, i)) return;
        positions.xAt(i) += velocities.xAt(i) * timeStep;
        positions.yAt(i) += velocities.yAt(i) * timeStep;
        if (!guardFinite(deformerReference().second, "springPendulum:positionUpdate", Vec2{positions.xAt(i), positions.yAt(i)}, i)) return;
    }
}

void ConnectedSpringPendulumDriver::getState(PhysicsDriverState& out) const {
    out.type = "SpringPendulum";
    out.lengths.clear();
    for (const auto value : lengths_) out.lengths.push_back(value);
    out.externalForce = externalForce_;
    out.damping = damping_;
    out.restore = restorationConstant_;
    out.timeStep = timeStep_;
    out.gravityVec = gravityVec_;
    out.springConstant = springConstant_;
    out.restorationConstant = restorationConstant_;
}

void ConnectedSpringPendulumDriver::setState(const PhysicsDriverState& state) {
    lengths_ = common::SharedArray<float>(state.lengths);
    externalForce_ = state.externalForce;
    damping_ = state.damping;
    restorationConstant_ = state.restorationConstant;
    if (restorationConstant_ == 0.0f) restorationConstant_ = state.restore;
    timeStep_ = state.timeStep;
    gravityVec_ = state.gravityVec;
    springConstant_ = state.springConstant;
}

void ConnectedPendulumDriver::retargetSource(std::shared_ptr<PathDeformer> deformer) {
    {
        [[maybe_unused]] const auto previousOwner = deformer_;
        deformer_ = deformer;
        nativeDeformer_ = deformer.get();
        nativeBorrowedDeformer_ = false;
    }
    setupSource();
}

void ConnectedPendulumDriver::setupSource() {
    if (d_runtime::ngRequireObject(deformerReference().second)->getVertices().size() < 2) return;
    updateDefaultShapeSource();
    if (!isCurrentDriver(deformerReference().second, this)) return;
    angles_ = initialAngles_.duplicate();
    angularVelocities_ = sourcePhysicsNewFloats(angles_.size());
    const auto range = angularVelocities_;
    for (std::size_t i = 0; i < range.size(); ++i) sourcePhysicsAt(angularVelocities_, i, 128) = 0;
    physDeformation_.resize(d_runtime::ngRequireObject(deformerReference().second)->getVertices().size());
    sourcePhysicsZero(physDeformation_);
    const auto curveX = d_runtime::ngRequireObject(d_runtime::ngRequireObject(deformerReference().second)->originalCurve);
    const float x = sourcePhysicsPoint(curveX->controlPoints(), 0).x;
    const auto curveY = d_runtime::ngRequireObject(d_runtime::ngRequireObject(deformerReference().second)->originalCurve);
    const float y = screenToPhysicsY(sourcePhysicsPoint(curveY->controlPoints(), 0).y);
    base_ = {x, y};
    if (!guardFinite(deformerReference().second, "pendulum:base", base_, 0)) return;
}

void ConnectedPendulumDriver::updateDefaultShapeSource() {
    common::Vec2Array physical;
    const auto curve = d_runtime::ngRequireObject(d_runtime::ngRequireObject(deformerReference().second)->originalCurve);
    const auto points = curve->controlPoints();
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto point = sourcePhysicsPoint(points, i);
        const Vec2 converted{point.x, screenToPhysicsY(point.y)};
        if (!guardFinite(deformerReference().second, "pendulum:controlPoint", converted, i)) return;
        physical.append(converted);
    }
    auto result = extractAnglesAndLengthsSource(physical);
    initialAngles_ = result.first;
    lengths_ = result.second;
    const auto angles = initialAngles_;
    for (auto angle : angles) {
        if (!guardFinite(deformerReference().second, "pendulum:initialAngle", angle)) {
            initialAngles_.resize(0); lengths_.resize(0); return;
        }
    }
    const auto lengths = lengths_;
    for (auto length : lengths) {
        if (!guardFinite(deformerReference().second, "pendulum:initialLength", length)) {
            initialAngles_.resize(0); lengths_.resize(0); return;
        }
    }
}

std::pair<common::SharedArray<float>, common::SharedArray<float>>
ConnectedPendulumDriver::extractAnglesAndLengthsSource(common::Vec2Array points) {
    common::SharedArray<float> angles, lengths;
    bool degenerate = false;
    for (std::int32_t cursor = 1; static_cast<std::size_t>(cursor) < points.size(); cursor = sourcePhysicsNext(cursor)) {
        const auto i = static_cast<std::size_t>(cursor);
        const auto b = sourcePhysicsPoint(points, i);
        const auto a = sourcePhysicsPoint(points, i - 1);
        const Vec2 vector{b.x - a.x, b.y - a.y};
        const float angle = std::atan2(vector.x, -vector.y);
        const float length = sourcePhysicsLength(vector);
        angles.push_back(angle);
        lengths.push_back(length);
        if (!std::isfinite(length) || length <= 1e-6f) {
            if (deformerReference().second) (void)deformerReference().second->getName();
            (void)sourcePhysicsPoint(points, i - 1);
            (void)sourcePhysicsPoint(points, i);
            degenerate = true;
        }
    }
    if (degenerate && deformerReference().second) deformerReference().second->reportPhysicsDegeneracy("pendulum:degenerateSegment");
    return {angles, lengths};
}

void ConnectedPendulumDriver::updatePendulumSource(common::SharedArray<float>& currentAngles,
    common::SharedArray<float>& angularVelocities, common::SharedArray<float> lengths,
    float damping, float restoreConstant, float baseVelocity, float step) {
    if (lengths.empty()) return;
    float externalTorque = externalForce_.x * step * sourcePhysicsAt(lengths, 0, 300);
    for (std::int32_t cursor = 0; static_cast<std::size_t>(cursor) < angles_.size(); cursor = sourcePhysicsNext(cursor)) {
        const auto i = static_cast<std::size_t>(cursor);
        if (!std::isfinite(sourcePhysicsAt(lengths, i, 302)) || sourcePhysicsAt(lengths, i, 302) <= 1e-6f) {
            if (deformerReference().second) (void)deformerReference().second->getName();
            (void)sourcePhysicsAt(lengths, i, 306);
            if (deformerReference().second) deformerReference().second->reportPhysicsDegeneracy("pendulum:zeroLength");
            return;
        }
        const float restore = -std::min(1.0f / step, restoreConstant) *
            (sourcePhysicsAt(currentAngles, i, 312) - sourcePhysicsAt(initialAngles_, i, 312));
        const float dampingTorque = -damping * sourcePhysicsAt(angularVelocities, i, 313);
        const float baseEffect = baseVelocity / sourcePhysicsAt(lengths, i, 314) *
            std::cos(sourcePhysicsAt(angles_, i, 314));
        const float gravityTorque = -gravity_ * std::sin(sourcePhysicsAt(angles_, i, 315) + worldAngle_);
        const float acceleration = restore + dampingTorque + baseEffect + gravityTorque + externalTorque;
        if (!std::isfinite(restore) || !std::isfinite(dampingTorque) || !std::isfinite(baseEffect) ||
            !std::isfinite(gravityTorque) || !std::isfinite(acceleration)) {
            if (deformerReference().second) deformerReference().second->reportPhysicsDegeneracy("pendulum:torqueNaN");
            return;
        }
        sourcePhysicsAt(angularVelocities, i, 323) += acceleration * step;
        if (!std::isfinite(sourcePhysicsAt(angularVelocities, i, 324))) {
            if (deformerReference().second) deformerReference().second->reportPhysicsDegeneracy("pendulum:velocityNaN");
            return;
        }
        sourcePhysicsAt(currentAngles, i, 330) += sourcePhysicsAt(angularVelocities, i, 330) * step;
        if (!std::isfinite(sourcePhysicsAt(currentAngles, i, 331))) {
            if (deformerReference().second) deformerReference().second->reportPhysicsDegeneracy("pendulum:angleNaN");
            return;
        }
        externalTorque += (-acceleration * std::sin(sourcePhysicsAt(currentAngles, i, 337))) *
            step * sourcePhysicsAt(lengths, i, 337) * propagateScale_;
    }
}

void ConnectedPendulumDriver::updateSource() {
    if (!isCurrentDriver(deformerReference().second, this)) return;
    if (d_runtime::ngRequireObject(deformerReference().second)->getVertices().size() < 2) return;
    if (!guardFinite(deformerReference().second, "pendulum:externalForce", externalForce_)) return;
    float remaining = static_cast<float>(std::min(deltaTime(), 10.0));
    while (static_cast<double>(remaining) > 0.01) {
        updatePendulumSource(angles_, angularVelocities_, lengths_, damping_, restoreConstant_, 0, 0.01f);
        if (!isCurrentDriver(deformerReference().second, this)) return;
        remaining = static_cast<float>(static_cast<double>(remaining) - 0.01);
    }
    updatePendulumSource(angles_, angularVelocities_, lengths_, damping_, restoreConstant_, 0, remaining);
    if (!isCurrentDriver(deformerReference().second, this)) return;
    const auto positions = sourcePhysicsPositions(base_, angles_, lengths_);
    for (std::size_t i = 0; i < positions.size(); ++i) {
        const auto point = sourcePhysicsPoint(positions, i);
        const Vec2 screen{point.x, physicsToScreenY(point.y)};
        if (!guardFinite(deformerReference().second, "pendulum:newPosition", screen, i)) {
            sourcePhysicsStore(physDeformation_, i, {0, 0}); continue;
        }
        const auto curve = d_runtime::ngRequireObject(d_runtime::ngRequireObject(deformerReference().second)->originalCurve);
        const auto original = sourcePhysicsPoint(curve->controlPoints(), i);
        const Vec2 difference{screen.x - original.x, screen.y - original.y};
        if (!guardFinite(deformerReference().second, "pendulum:newDelta", difference, i)) {
            sourcePhysicsStore(physDeformation_, i, {0, 0}); continue;
        }
        sourcePhysicsStore(physDeformation_, i, difference);
    }
    for (std::int32_t cursor = 0; static_cast<std::size_t>(cursor) < physDeformation_.size(); cursor = sourcePhysicsNext(cursor)) {
        const auto i = static_cast<std::size_t>(cursor);
        const auto delta = sourcePhysicsPoint(physDeformation_, i);
        if (!guardFinite(deformerReference().second, "pendulum:physDeformation", delta, i)) continue;
        auto& destination = d_runtime::ngRequireObject(deformerReference().second)->deformation;
        const auto previous = sourcePhysicsPoint(destination, i);
        sourcePhysicsStore(destination, i, {previous.x + delta.x, previous.y + delta.y});
    }
}

void ConnectedPendulumDriver::copyFromSource(ConnectedPhysicsDriver* source) {
    if (const auto driver = dynamic_cast<ConnectedPendulumDriver*>(source)) {
        damping_ = driver->damping_;
        restoreConstant_ = driver->restoreConstant_;
        gravity_ = driver->gravity_;
        inputScale_ = driver->inputScale_;
        propagateScale_ = driver->propagateScale_;
    }
}

void ConnectedSpringPendulumDriver::retargetSource(std::shared_ptr<PathDeformer> deformer) {
    {
        [[maybe_unused]] const auto previousOwner = deformer_;
        deformer_ = deformer;
        nativeDeformer_ = deformer.get();
        nativeBorrowedDeformer_ = false;
    }
    setupSource();
}

void ConnectedSpringPendulumDriver::updateDefaultShapeSource() {
    const auto curve = d_runtime::ngRequireObject(d_runtime::ngRequireObject(deformerReference().second)->originalCurve);
    const auto original = curve->controlPoints();
    initialPositions_.resize(original.size());
    for (std::size_t i = 0; i < original.size(); ++i) {
        auto point = sourcePhysicsPoint(original, i);
        point.y = screenToPhysicsY(point.y);
        if (!guardFinite(deformerReference().second, "springPendulum:controlPoint", point, i)) {
            initialPositions_.resize(0); return;
        }
        sourcePhysicsStore(initialPositions_, i, point);
    }
}

void ConnectedSpringPendulumDriver::setupSource() {
    updateDefaultShapeSource();
    if (!deformerReference().second) return;
    positions_ = initialPositions_.dup();
    velocities_.resize(positions_.size());
    sourcePhysicsZero(velocities_);
    physDeformation_.resize(d_runtime::ngRequireObject(deformerReference().second)->getVertices().size());
    sourcePhysicsZero(physDeformation_);
    if (positions_.empty()) ngThrowPathDriverAllocationError();
    lengths_ = sourcePhysicsNewFloats(positions_.size() - 1);
    bool degenerate = false;
    for (std::int32_t cursor = 0; static_cast<std::size_t>(cursor) < lengths_.size(); cursor = sourcePhysicsNext(cursor)) {
        const auto i = static_cast<std::size_t>(cursor);
        if (!guardFinite(deformerReference().second, "springPendulum:position", sourcePhysicsPoint(positions_, i), i)) {
            degenerate = true; break;
        }
        if (!guardFinite(deformerReference().second, "springPendulum:position", sourcePhysicsPoint(positions_, i + 1), i + 1)) {
            degenerate = true; break;
        }
        const auto next = sourcePhysicsPoint(positions_, i + 1);
        const auto previous = sourcePhysicsPoint(positions_, i);
        const float length = sourcePhysicsLength({next.x - previous.x, next.y - previous.y});
        sourcePhysicsAt(lengths_, i, 403) = length;
        if (!std::isfinite(length) || length <= 1e-6f) {
            if (deformerReference().second) (void)deformerReference().second->getName();
            (void)sourcePhysicsPoint(positions_, i);
            (void)sourcePhysicsPoint(positions_, i + 1);
            degenerate = true;
        }
    }
    if (degenerate && deformerReference().second) {
        deformerReference().second->reportPhysicsDegeneracy("springPendulum:degenerateSegment"); return;
    }
    if (!isCurrentDriver(deformerReference().second, this)) return;
}

void ConnectedSpringPendulumDriver::updateSpringPendulumSource(common::Vec2Array& positions,
    common::Vec2Array& velocities, common::Vec2Array initial, common::SharedArray<float> lengths,
    float damping, float spring, float restoration, float step) {
    for (std::int32_t cursor = 1; static_cast<std::size_t>(cursor) < positions.size(); cursor = sourcePhysicsNext(cursor)) {
        const auto i = static_cast<std::size_t>(cursor);
        Vec2 force{0, 0};
        if (i > 0) {
            const auto point = sourcePhysicsPoint(positions, i);
            auto previous = sourcePhysicsPoint(positions, i - 1);
            if (i == 1) { previous.x += externalForce_.x * step; previous.y += externalForce_.y * step; }
            const Vec2 difference{point.x - previous.x, point.y - previous.y};
            if (!guardFinite(deformerReference().second, "springPendulum:diffPrev", difference, i)) return;
            const float length = sourcePhysicsLength(difference);
            if (!std::isfinite(length) || length <= 1e-6f) {
                if (deformerReference().second) (void)deformerReference().second->getName();
                (void)sourcePhysicsAt(lengths, i - 1, 536);
                if (deformerReference().second) deformerReference().second->reportPhysicsDegeneracy("springPendulum:zeroDiff");
                return;
            }
            const float offset = length - sourcePhysicsAt(lengths, i - 1, 542);
            force.x += (difference.x * offset / length) * (-spring);
            force.y += (difference.y * offset / length) * (-spring);
        }
        if (i < positions.size() - 1) {
            auto current = sourcePhysicsPoint(positions, i);
            if (i == 0) { current.x += externalForce_.x * step; current.y += externalForce_.y * step; }
            const auto next = sourcePhysicsPoint(positions, i + 1);
            const Vec2 difference{current.x - next.x, current.y - next.y};
            if (!guardFinite(deformerReference().second, "springPendulum:diffNext", difference, i)) return;
            const float length = sourcePhysicsLength(difference);
            if (!std::isfinite(length) || length <= 1e-6f) {
                if (deformerReference().second) (void)deformerReference().second->getName();
                (void)sourcePhysicsAt(lengths, i, 555);
                if (deformerReference().second) deformerReference().second->reportPhysicsDegeneracy("springPendulum:zeroDiff");
                return;
            }
            const float offset = length - sourcePhysicsAt(lengths, i, 561);
            force.x += (difference.x * offset / length) * (-spring);
            force.y += (difference.y * offset / length) * (-spring);
        }
        const auto current = sourcePhysicsPoint(positions, i);
        const auto reference = sourcePhysicsPoint(initial, i);
        const Vec2 restoring{(current.x - reference.x) * (-restoration), (current.y - reference.y) * (-restoration)};
        const auto velocity = sourcePhysicsPoint(velocities, i);
        const Vec2 dampingForce{velocity.x * (-damping), velocity.y * (-damping)};
        // inmath's scalar parameter has the vector element type, including /1.0.
        const Vec2 acceleration{
            (force.x + dampingForce.x + restoring.x + gravityVec_.x) / 1.0f,
            (force.y + dampingForce.y + restoring.y + gravityVec_.y) / 1.0f};
        if (!guardFinite(deformerReference().second, "springPendulum:acceleration", acceleration, i)) return;
        sourcePhysicsStore(velocities, i, {velocity.x + acceleration.x * step, velocity.y + acceleration.y * step});
        if (!guardFinite(deformerReference().second, "springPendulum:velocity", sourcePhysicsPoint(velocities, i), i)) return;
        const auto updatedVelocity = sourcePhysicsPoint(velocities, i);
        const auto previousPosition = sourcePhysicsPoint(positions, i);
        sourcePhysicsStore(positions, i,
            {previousPosition.x + updatedVelocity.x * step, previousPosition.y + updatedVelocity.y * step});
        if (!guardFinite(deformerReference().second, "springPendulum:positionUpdate", sourcePhysicsPoint(positions, i), i)) return;
    }
}

void ConnectedSpringPendulumDriver::updateSource() {
    if (!isCurrentDriver(deformerReference().second, this)) return;
    float remaining = static_cast<float>(std::min(deltaTime(), 10.0));
    if (!guardFinite(deformerReference().second, "springPendulum:externalForce", externalForce_)) return;
    while (static_cast<double>(remaining) > 0.01) {
        updateSpringPendulumSource(positions_, velocities_, initialPositions_, lengths_, damping_,
            springConstant_, restorationConstant_, 0.01f);
        if (!isCurrentDriver(deformerReference().second, this)) return;
        remaining = static_cast<float>(static_cast<double>(remaining) - 0.01);
    }
    updateSpringPendulumSource(positions_, velocities_, initialPositions_, lengths_, damping_,
        springConstant_, restorationConstant_, remaining);
    if (!isCurrentDriver(deformerReference().second, this)) return;
    for (std::int32_t cursor = 0; static_cast<std::size_t>(cursor) < positions_.size(); cursor = sourcePhysicsNext(cursor)) {
        const auto i = static_cast<std::size_t>(cursor);
        const auto point = sourcePhysicsPoint(positions_, i);
        const Vec2 screen{point.x, physicsToScreenY(point.y)};
        if (!std::isfinite(screen.x) || !std::isfinite(screen.y)) {
            if (deformerReference().second) deformerReference().second->reportDriverInvalidSource("springPendulum:position", i, screen);
            sourcePhysicsStore(physDeformation_, i, {0, 0}); continue;
        }
        const auto curve = d_runtime::ngRequireObject(d_runtime::ngRequireObject(deformerReference().second)->originalCurve);
        const auto original = sourcePhysicsPoint(curve->controlPoints(), i);
        const Vec2 difference{screen.x - original.x, screen.y - original.y};
        if (!std::isfinite(difference.x) || !std::isfinite(difference.y)) {
            if (deformerReference().second) deformerReference().second->reportDriverInvalidSource("springPendulum:delta", i, difference);
            sourcePhysicsStore(physDeformation_, i, {0, 0}); continue;
        }
        sourcePhysicsStore(physDeformation_, i, difference);
    }
    for (std::int32_t cursor = 0; static_cast<std::size_t>(cursor) < physDeformation_.size(); cursor = sourcePhysicsNext(cursor)) {
        const auto i = static_cast<std::size_t>(cursor);
        const auto delta = sourcePhysicsPoint(physDeformation_, i);
        if (!std::isfinite(delta.x) || !std::isfinite(delta.y)) continue;
        auto& destination = d_runtime::ngRequireObject(deformerReference().second)->deformation;
        const auto previous = sourcePhysicsPoint(destination, i);
        sourcePhysicsStore(destination, i, {previous.x + delta.x, previous.y + delta.y});
    }
}

void ConnectedSpringPendulumDriver::copyFromSource(ConnectedPhysicsDriver*) {
    // The original class intentionally has an empty copyFrom body.
}

void ConnectedPendulumDriver::serializeSource(core::serde::TypedJsonWriter& writer) const {
    writer.putField("type", "Pendulum");
    writer.putField("damping", damping_);
    writer.putField("restore_constant", restoreConstant_);
    writer.putField("gravity", gravity_);
    writer.putField("input_scale", inputScale_);
    writer.putField("propagate_scale", propagateScale_);
}

core::serde::ReadError ConnectedPendulumDriver::deserializeSource(core::serde::TypedJsonView data) {
    if (data.isMissing()) return {};
    const auto read = [&](std::string_view key, float& value) -> core::serde::ReadError {
        const auto field = data.findFirst(key);
        if (field.isMissing()) return {};
        return core::serde::deserializeValue(field, value);
    };
    if (auto error = read("damping", damping_)) return error;
    if (auto error = read("restore_constant", restoreConstant_)) return error;
    if (auto error = read("gravity", gravity_)) return error;
    if (auto error = read("input_scale", inputScale_)) return error;
    if (auto error = read("propagate_scale", propagateScale_)) return error;
    return {};
}

void ConnectedSpringPendulumDriver::serializeSource(core::serde::TypedJsonWriter& writer) const {
    writer.putField("type", "SpringPendulum");
}

core::serde::ReadError ConnectedSpringPendulumDriver::deserializeSource(core::serde::TypedJsonView) {
    // The authored SpringPendulum implementation accepts the value unchanged.
    return {};
}

} // namespace nicxlive::core::nodes
