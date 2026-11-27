#include "camera.hpp"

#include "../runtime_state.hpp"
#include "../common/object_key_policy.hpp"
#include "../compat/throwable.hpp"

#include <cmath>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>

namespace nicxlive::core::math {

namespace {
void prepareCameraClassInfo() {
    static const bool registered = [] {
        d_runtime::ngRegisterObjectClassName(typeid(Camera),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.math.camera.Camera"));
        return true;
    }();
    (void)registered;
}
}

// Object defaults, object.d (Digital Mars 2000-2011), BSL-1.0.
// Complete notice: core/licenses/druntime-associative-array.txt.
d_runtime::RetainedString Camera::toString() {
    const auto receiver = weak_from_this().lock();
    prepareCameraClassInfo();
    return d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t Camera::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool Camera::opEquals(std::shared_ptr<Camera> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int Camera::opCmp(std::shared_ptr<Camera>) {
    const auto receiver = weak_from_this().lock();
    prepareCameraClassInfo();
    const auto className = d_runtime::ngObjectClassName(typeid(*this));
    const auto message = [&] {
        try {
            std::string text = "need opCmp for class ";
            text += className.view();
            return d_runtime::RetainedString(std::move(text));
        } catch (const std::bad_alloc&) {
            throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
        }
    }();
    throw d_runtime::ThrowableException(message,
        d_runtime::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool ngCameraObjectEquals(std::shared_ptr<Camera> left, std::shared_ptr<Camera> right) {
    return common::ngClassObjectEquals(std::move(left), std::move(right), &prepareCameraClassInfo);
}

Vec2 Camera::getRealSize() const {
    int w = 0, h = 0;
    ::nicxlive::core::inGetViewport(w, h);
    Vec2 real{};
    real.x = static_cast<float>(w) / scale.x;
    real.y = static_cast<float>(h) / scale.y;
    return real;
}

Vec2 Camera::getCenterOffset() const {
    Vec2 rs = getRealSize();
    return Vec2{rs.x / 2.0f, rs.y / 2.0f};
}

Mat4 Camera::matrix() const {
    // The authored Camera normalizes stored state before producing its matrix.
    if (!std::isfinite(position.x) || !std::isfinite(position.y)) position = {0, 0};
    if (!std::isfinite(scale.x) || !std::isfinite(scale.y)) scale = {1, 1};
    if (!std::isfinite(rotation)) rotation = 0;
    Vec2 realSize = getRealSize();
    if (!std::isfinite(realSize.x) || !std::isfinite(realSize.y)) {
        return Mat4::identity();
    }
    Vec2 origin{realSize.x / 2.0f, realSize.y / 2.0f};
    constexpr float kMaxDepth = 65535.0f;
    const float z = -static_cast<float>(std::numeric_limits<uint16_t>::max() / 2);

    Mat4 ortho = Mat4::orthographic(0.0f, realSize.x, realSize.y, 0.0f, 0.0f, kMaxDepth);
    Mat4 tOrigin = Mat4::translation(origin.x, origin.y, 0.0f);
    Mat4 rot = Mat4::zRotation(rotation);
    Mat4 tPos = Mat4::translation(position.x, position.y, z);
    return ortho * tOrigin * rot * tPos;
}


// Mutable API from nijilive/math/camera.d. Author: Luna Nielsen.
// Copyright (c) 2020 Inochi2D Project, 2024 nijigenerate Project.
// BSD-2-Clause; see the SDK LICENSE.
Vec2 Camera::getRealSize() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    int width = 0, height = 0;
    ::nicxlive::core::inGetViewport(width, height);
    return Vec2{static_cast<float>(width) / scale.x, static_cast<float>(height) / scale.y};
}

Vec2 Camera::getCenterOffset() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const Vec2 realSize = getRealSize();
    return Vec2{realSize.x / 2.0f, realSize.y / 2.0f};
}

Mat4 Camera::matrix() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!std::isfinite(position.x) || !std::isfinite(position.y)) position = {0, 0};
    if (!std::isfinite(scale.x) || !std::isfinite(scale.y)) scale = {1, 1};
    if (!std::isfinite(rotation)) rotation = 0;
    const Vec2 realSize = getRealSize();
    if (!std::isfinite(realSize.x) || !std::isfinite(realSize.y)) return Mat4::identity();

    const Vec2 origin{realSize.x / 2.0f, realSize.y / 2.0f};
    constexpr float maxDepth = 65535.0f;
    const Vec3 pos{position.x, position.y,
        -static_cast<float>(std::numeric_limits<std::uint16_t>::max() / 2)};

    const auto projection = Mat4::orthographic(0.0f, realSize.x, realSize.y, 0.0f, 0.0f, maxDepth);
    const auto centered = projection * Mat4::translation(origin.x, origin.y, 0.0f);
    const auto rotated = centered * Mat4::zRotation(rotation);
    return rotated * Mat4::translation(pos.x, pos.y, pos.z);
}

} // namespace nicxlive::core::math
