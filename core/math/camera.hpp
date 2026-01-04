#pragma once

#include "types.hpp"
#include "mat4.hpp"

namespace nicxlive::core::math {

class Camera {
public:
    Vec2 position{0.0f, 0.0f};
    float rotation{0.0f};
    Vec2 scale{1.0f, 1.0f};

    Vec2 getRealSize() const;
    Vec2 getCenterOffset() const;
    Mat4 matrix() const;
};

} // namespace nicxlive::core::math
