#pragma once
// Concrete public systems from nijilive/core/nodes/drivers/simplephysics.d.
// Copyright Inochi2D/nijigenerate contributors, BSD-2-Clause; see SDK LICENSE.
#include "simple_physics_driver.hpp"
#include "../phys/system.hpp"
#include "../dbg.hpp"
#include "../compat/native_access_fault.hpp"
#include <cmath>
#include <limits>
#include <memory>

namespace nicxlive::core::nodes {

class Pendulum : public ::nicxlive::phys::PhysicsSystem {
public:
    std::shared_ptr<SimplePhysicsDriver> driver;
    Pendulum(const Pendulum&) = delete;
    Pendulum& operator=(const Pendulum&) = delete;
    Pendulum(Pendulum&&) = delete;
    Pendulum& operator=(Pendulum&&) = delete;

    explicit Pendulum(std::shared_ptr<SimplePhysicsDriver> value) { constructOwned(value); }
    static std::shared_ptr<Pendulum> create(std::shared_ptr<SimplePhysicsDriver> value) {
        auto result = std::allocate_shared<Pendulum>(d_runtime::SourceAllocator<Pendulum>{}, Deferred{});
        result->constructOwned(value);
        return result;
    }

    void tick(float h) override {
        [[maybe_unused]] const auto lifetime = weak_from_this().lock();
        const auto anchor = d_runtime::ngRequireObject(driver)->anchor;
        Vec2 dBob{bob.x - anchor.x, bob.y - anchor.y};
        if (!finite(dBob)) return;
        angle = std::atan2(-dBob.x, dBob.y);
        if (!std::isfinite(angle)) return;
        ::nicxlive::phys::PhysicsSystem::tick(h);
        if (!std::isfinite(angle) || !std::isfinite(dAngle)) return;
        dBob = {-std::sin(angle), std::cos(angle)};
        if (!finite(dBob)) return;
        const float length = d_runtime::ngRequireObject(driver)->getLength();
        if (!std::isfinite(length) || std::fabs(length) <= std::numeric_limits<float>::epsilon()) return;
        const auto currentAnchor = d_runtime::ngRequireObject(driver)->anchor;
        bob = {currentAnchor.x + dBob.x * length, currentAnchor.y + dBob.y * length};
        if (!finite(bob)) return;
        d_runtime::ngRequireObject(driver)->output = bob;
    }

    void drawDebug(Mat4 transform = Mat4::identity()) override {
        [[maybe_unused]] const auto lifetime = weak_from_this().lock();
        const float x = d_runtime::ngRequireObject(driver)->anchor.x;
        const float y = d_runtime::ngRequireObject(driver)->anchor.y;
        Vec3Array points(common::SharedArray<Vec3>{{x, y, 0}, {bob.x, bob.y, 0}});
        dbg::inDbgSetBuffer(points);
        dbg::inDbgLineWidth(3);
        dbg::inDbgDrawLines({1, 0, 1, 1}, transform);
    }

    void updateAnchor() override {
        [[maybe_unused]] const auto lifetime = weak_from_this().lock();
        const auto anchor = d_runtime::ngRequireObject(driver)->anchor;
        const float length = d_runtime::ngRequireObject(driver)->getLength();
        bob = {anchor.x + 0.0f, anchor.y + length};
    }

protected:
    void eval(float) override {
        [[maybe_unused]] const auto lifetime = weak_from_this().lock();
        setD(angle, dAngle);
        const float gravity = d_runtime::ngRequireObject(driver)->getGravity();
        const float length = d_runtime::ngRequireObject(driver)->getLength();
        if (!std::isfinite(gravity) || !std::isfinite(length) ||
            std::fabs(length) <= std::numeric_limits<float>::epsilon()) {
            setD(dAngle, 0); return;
        }
        const float ratio = gravity / length;
        if (!std::isfinite(ratio) || ratio < 0) { setD(dAngle, 0); return; }
        const float critical = 2 * std::sqrt(ratio);
        if (!std::isfinite(critical)) { setD(dAngle, 0); return; }
        const float damping = d_runtime::ngRequireObject(driver)->getAngleDamping();
        if (!std::isfinite(damping)) { setD(dAngle, 0); return; }
        float dd = -ratio * std::sin(angle);
        if (!std::isfinite(dd)) dd = 0;
        dd -= dAngle * damping * critical;
        if (!std::isfinite(dd)) dd = 0;
        setD(dAngle, dd);
    }

private:
    template<class> friend struct d_runtime::SourceAllocator;
    struct Deferred {};
    explicit Pendulum(Deferred) {}
    void constructOwned(std::shared_ptr<SimplePhysicsDriver> value) {
        driver = value;
        const auto anchor = d_runtime::ngRequireObject(value)->anchor;
        const float length = d_runtime::ngRequireObject(value)->getLength();
        bob = {anchor.x + 0.0f, anchor.y + length};
        addVariable(&angle);
        addVariable(&dAngle);
    }
    Vec2 bob{0, 0};
    float angle{0};
    float dAngle{0};
    static bool finite(Vec2 value) { return std::isfinite(value.x) && std::isfinite(value.y); }
};

class SpringPendulum : public ::nicxlive::phys::PhysicsSystem {
public:
    std::shared_ptr<SimplePhysicsDriver> driver;
    SpringPendulum(const SpringPendulum&) = delete;
    SpringPendulum& operator=(const SpringPendulum&) = delete;
    SpringPendulum(SpringPendulum&&) = delete;
    SpringPendulum& operator=(SpringPendulum&&) = delete;

    explicit SpringPendulum(std::shared_ptr<SimplePhysicsDriver> value) { constructOwned(value); }
    static std::shared_ptr<SpringPendulum> create(std::shared_ptr<SimplePhysicsDriver> value) {
        auto result = std::allocate_shared<SpringPendulum>(
            d_runtime::SourceAllocator<SpringPendulum>{}, Deferred{});
        result->constructOwned(value);
        return result;
    }

    void tick(float h) override {
        [[maybe_unused]] const auto lifetime = weak_from_this().lock();
        ::nicxlive::phys::PhysicsSystem::tick(h);
        if (!finite(bob) || !finite(dBob)) return;
        if (!finite(d_runtime::ngRequireObject(driver)->anchor)) return;
        d_runtime::ngRequireObject(driver)->output = bob;
    }

    void drawDebug(Mat4 transform = Mat4::identity()) override {
        [[maybe_unused]] const auto lifetime = weak_from_this().lock();
        const float x = d_runtime::ngRequireObject(driver)->anchor.x;
        const float y = d_runtime::ngRequireObject(driver)->anchor.y;
        Vec3Array points(common::SharedArray<Vec3>{{x, y, 0}, {bob.x, bob.y, 0}});
        dbg::inDbgSetBuffer(points);
        dbg::inDbgLineWidth(3);
        dbg::inDbgDrawLines({1, 0, 1, 1}, transform);
    }

    void updateAnchor() override {
        [[maybe_unused]] const auto lifetime = weak_from_this().lock();
        const auto anchor = d_runtime::ngRequireObject(driver)->anchor;
        const float length = d_runtime::ngRequireObject(driver)->getLength();
        bob = {anchor.x + 0.0f, anchor.y + length};
    }

protected:
    void eval(float) override {
        [[maybe_unused]] const auto lifetime = weak_from_this().lock();
        setD(bob, dBob);
        const float frequency = d_runtime::ngRequireObject(driver)->getFrequency();
        if (!std::isfinite(frequency) || std::fabs(frequency) <= std::numeric_limits<float>::epsilon()) {
            setD(dBob, Vec2{0, 0}); return;
        }
        const float rootK = static_cast<float>(frequency * 2 * detail::physicsPi);
        if (!std::isfinite(rootK)) { setD(dBob, Vec2{0, 0}); return; }
        const float k = rootK * rootK;
        if (!std::isfinite(k) || std::fabs(k) <= std::numeric_limits<float>::epsilon()) {
            setD(dBob, Vec2{0, 0}); return;
        }
        const float gravity = d_runtime::ngRequireObject(driver)->getGravity();
        if (!std::isfinite(gravity)) { setD(dBob, Vec2{0, 0}); return; }
        const float length = d_runtime::ngRequireObject(driver)->getLength();
        if (!std::isfinite(length) || std::fabs(length) <= std::numeric_limits<float>::epsilon()) {
            setD(dBob, Vec2{0, 0}); return;
        }
        const float restLength = length - gravity / k;
        if (!std::isfinite(restLength)) { setD(dBob, Vec2{0, 0}); return; }
        const auto anchor = d_runtime::ngRequireObject(driver)->anchor;
        const Vec2 off{bob.x - anchor.x, bob.y - anchor.y};
        if (!finite(off)) { setD(dBob, Vec2{0, 0}); return; }
        const float offLength = std::sqrt(off.x * off.x + off.y * off.y);
        Vec2 normal;
        if (!std::isfinite(offLength)) { setD(dBob, Vec2{0, 0}); return; }
        if (offLength <= std::numeric_limits<float>::epsilon()) {
            normal = finite(lastValidOffPosNorm) ? lastValidOffPosNorm : Vec2{0, 1};
        } else {
            normal = {off.x / offLength, off.y / offLength};
            if (!finite(normal)) { setD(dBob, Vec2{0, 0}); return; }
            lastValidOffPosNorm = normal;
        }
        const float ratio = gravity / length;
        if (!std::isfinite(ratio) || ratio < 0) { setD(dBob, Vec2{0, 0}); return; }
        const float criticalAngle = 2 * std::sqrt(ratio);
        const float criticalLength = 2 * rootK;
        if (!std::isfinite(criticalAngle) || !std::isfinite(criticalLength)) {
            setD(dBob, Vec2{0, 0}); return;
        }
        const float distance = std::fabs(offLength);
        if (!std::isfinite(distance)) { setD(dBob, Vec2{0, 0}); return; }
        Vec2 ddBob{0.0f - normal.x * (distance - restLength) * k,
            gravity - normal.y * (distance - restLength) * k};
        if (!finite(ddBob)) ddBob = {0, 0};
        const Vec2 velocity{dBob.x * normal.y + dBob.y * normal.x,
            dBob.y * normal.y - dBob.x * normal.x};
        const float angleDamping = d_runtime::ngRequireObject(driver)->getAngleDamping();
        const float lengthDamping = d_runtime::ngRequireObject(driver)->getLengthDamping();
        if (!std::isfinite(angleDamping) || !std::isfinite(lengthDamping)) {
            setD(dBob, Vec2{0, 0}); return;
        }
        Vec2 rotation{-(velocity.x * angleDamping * criticalAngle),
            -(velocity.y * lengthDamping * criticalLength)};
        if (!finite(rotation)) rotation = {0, 0};
        // Preserve the original use of velocity, rather than rotation, in these two terms.
        Vec2 damping{rotation.x * normal.y - velocity.y * normal.x,
            rotation.y * normal.y + velocity.x * normal.x};
        if (!finite(damping)) damping = {0, 0};
        ddBob = {ddBob.x + damping.x, ddBob.y + damping.y};
        if (!finite(ddBob)) ddBob = {0, 0};
        setD(dBob, ddBob);
    }

private:
    template<class> friend struct d_runtime::SourceAllocator;
    struct Deferred {};
    explicit SpringPendulum(Deferred) {}
    void constructOwned(std::shared_ptr<SimplePhysicsDriver> value) {
        driver = value;
        const auto anchor = d_runtime::ngRequireObject(value)->anchor;
        const float length = d_runtime::ngRequireObject(value)->getLength();
        bob = {anchor.x + 0.0f, anchor.y + length};
        addVariable(&bob);
        addVariable(&dBob);
    }
    Vec2 bob{0, 0};
    Vec2 dBob{0, 0};
    Vec2 lastValidOffPosNorm{0, 1};
    static bool finite(Vec2 value) { return std::isfinite(value.x) && std::isfinite(value.y); }
};

} // namespace nicxlive::core::nodes
