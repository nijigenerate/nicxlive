#pragma once
#include <cstdint>

namespace nicxlive::core::math {

struct Vec2 {
    float x{0.0f};
    float y{0.0f};
};

inline Vec2 operator+(const Vec2& lhs, const Vec2& rhs) {
    return Vec2{lhs.x + rhs.x, lhs.y + rhs.y};
}

inline Vec2 operator-(const Vec2& lhs, const Vec2& rhs) {
    return Vec2{lhs.x - rhs.x, lhs.y - rhs.y};
}

inline Vec2 operator*(const Vec2& value, float scale) {
    return Vec2{value.x * scale, value.y * scale};
}

inline Vec2 operator*(float scale, const Vec2& value) {
    return value * scale;
}

struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

inline Vec3 operator+(const Vec3& lhs, const Vec3& rhs) {
    return Vec3{lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

inline Vec3 operator-(const Vec3& lhs, const Vec3& rhs) {
    return Vec3{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

inline Vec3 operator*(const Vec3& value, float scale) {
    return Vec3{value.x * scale, value.y * scale, value.z * scale};
}

inline Vec3 operator*(float scale, const Vec3& value) {
    return value * scale;
}

struct Vec4 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float w{0.0f};
};

inline Vec4 operator+(const Vec4& lhs, const Vec4& rhs) {
    return Vec4{lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w};
}

inline Vec4 operator-(const Vec4& lhs, const Vec4& rhs) {
    return Vec4{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w};
}

// Required inmath Vector!(int, 2/4) field and equality surface.
// Signed coordinates default to int.init (zero); no float API is changed.
struct Vec2i {
    using vt = std::int32_t;
    static constexpr int dimension = 2;
    std::int32_t x{0};
    std::int32_t y{0};
    bool operator==(const Vec2i&) const = default;
};

struct Vec4i {
    using vt = std::int32_t;
    static constexpr int dimension = 4;
    std::int32_t x{0};
    std::int32_t y{0};
    std::int32_t z{0};
    std::int32_t w{0};
    bool operator==(const Vec4i&) const = default;
};

} // namespace nicxlive::core::math
