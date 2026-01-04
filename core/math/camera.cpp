#include "camera.hpp"

#include "../runtime_state.hpp"

#include <cmath>

namespace nicxlive::core::math {

Vec2 Camera::getRealSize() const {
    int w = 0, h = 0;
    ::nicxlive::core::inGetViewport(w, h);
    Vec2 real{};
    real.x = (scale.x != 0.0f) ? static_cast<float>(w) / scale.x : 0.0f;
    real.y = (scale.y != 0.0f) ? static_cast<float>(h) / scale.y : 0.0f;
    return real;
}

Vec2 Camera::getCenterOffset() const {
    Vec2 rs = getRealSize();
    return Vec2{rs.x / 2.0f, rs.y / 2.0f};
}

Mat4 Camera::matrix() const {
    Vec2 realSize = getRealSize();
    if (!std::isfinite(realSize.x) || !std::isfinite(realSize.y) || realSize.x == 0.0f || realSize.y == 0.0f) {
        return Mat4::identity();
    }
    Vec2 origin{realSize.x / 2.0f, realSize.y / 2.0f};
    constexpr float kMaxDepth = 65535.0f;
    const float z = -(kMaxDepth / 2.0f);

    Mat4 ortho = Mat4::orthographic(0.0f, realSize.x, realSize.y, 0.0f, 0.0f, kMaxDepth);
    Mat4 tOrigin = Mat4::translation(origin.x, origin.y, 0.0f);
    Mat4 rot = Mat4::zRotation(rotation);
    Mat4 tPos = Mat4::translation(position.x, position.y, z);
    return ortho * tOrigin * rot * tPos;
}

} // namespace nicxlive::core::math
