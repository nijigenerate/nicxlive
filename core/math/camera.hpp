#pragma once

#include "types.hpp"
#include "mat4.hpp"
#include "../compat/retained_string.hpp"
#include "../compat/object_metadata.hpp"
#include <cstddef>
#include <memory>
#include <limits>

namespace nicxlive::core::math {

class Camera : public std::enable_shared_from_this<Camera> {
private:
    // D's private mat4 field retains the default float NaN initialization.
    Mat4 projection = [] {
        Mat4 value;
        for (auto& row : value.a.a)
            for (auto& cell : row) cell = std::numeric_limits<float>::quiet_NaN();
        return value;
    }();
public:
    virtual ~Camera() = default;
    mutable Vec2 position{0.0f, 0.0f};
    mutable float rotation{0.0f};
    mutable Vec2 scale{1.0f, 1.0f};

    Vec2 getRealSize() const;
    Vec2 getCenterOffset() const;
    virtual Mat4 matrix() const;

    // Mutable Source API; existing const virtual slots remain in their original order.
    virtual Vec2 getRealSize();
    virtual Vec2 getCenterOffset();
    virtual Mat4 matrix();

    // Inherited Source Object methods, appended after all existing virtual slots.
    virtual d_runtime::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<Camera> other);
    virtual int opCmp(std::shared_ptr<Camera> other);
};

bool ngCameraObjectEquals(std::shared_ptr<Camera> left, std::shared_ptr<Camera> right);

} // namespace nicxlive::core::math

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<math::Camera> {
    inline static constexpr char className[] = "nijilive.math.camera.Camera";
    inline static constexpr char templateArgumentName[] = "(nijilive.math.camera.Camera)";
};
template<> struct SourceTypeSpellings<math::Camera> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijilive.math.camera.Camera)";
    inline static constexpr char unqualifiedArgument[] = "(Camera)";
    inline static constexpr char qualifiedElement[] = "nijilive.math.camera.Camera";
    inline static constexpr char unqualifiedElement[] = "Camera";
};
} // namespace nicxlive::core::d_runtime
