#include "triangle.hpp"
#include "../nodes/drawable.hpp"

#include <algorithm>
#include <cmath>

namespace nicxlive::core::math {
namespace {
using ::nicxlive::core::nodes::Vec2;
using ::nicxlive::core::nodes::Vec2Array;
using ::nicxlive::core::nodes::MeshData;

float dot(const Vec2& a, const Vec2& b) {
    return a.x * b.x + a.y * b.y;
}

float length(const Vec2& v) {
    return std::sqrt(dot(v, v));
}

Vec2 add(const Vec2& a, const Vec2& b) {
    return Vec2{a.x + b.x, a.y + b.y};
}

Vec2 sub(const Vec2& a, const Vec2& b) {
    return Vec2{a.x - b.x, a.y - b.y};
}

Vec2 mul(const Vec2& a, float s) {
    return Vec2{a.x * s, a.y * s};
}

float calculateAngle(const Vec2& a, const Vec2& b) {
    return std::atan2(b.y - a.y, b.x - a.x);
}

struct Mat3 {
    float m[3][3]{};
};

Mat3 mulMat(const Mat3& a, const Mat3& b) {
    Mat3 out{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            out.m[i][j] = 0.0f;
            for (int k = 0; k < 3; ++k) {
                out.m[i][j] += a.m[i][k] * b.m[k][j];
            }
        }
    }
    return out;
}

float detMat(const Mat3& a) {
    return a.m[0][0] * (a.m[1][1] * a.m[2][2] - a.m[1][2] * a.m[2][1])
         - a.m[0][1] * (a.m[1][0] * a.m[2][2] - a.m[1][2] * a.m[2][0])
         + a.m[0][2] * (a.m[1][0] * a.m[2][1] - a.m[1][1] * a.m[2][0]);
}

Mat3 invMat(const Mat3& a) {
    Mat3 out{};
    const float det = detMat(a);
    if (std::abs(det) < 1e-8f) {
        return out;
    }
    const float invDet = 1.0f / det;
    out.m[0][0] =  (a.m[1][1] * a.m[2][2] - a.m[1][2] * a.m[2][1]) * invDet;
    out.m[0][1] = -(a.m[0][1] * a.m[2][2] - a.m[0][2] * a.m[2][1]) * invDet;
    out.m[0][2] =  (a.m[0][1] * a.m[1][2] - a.m[0][2] * a.m[1][1]) * invDet;
    out.m[1][0] = -(a.m[1][0] * a.m[2][2] - a.m[1][2] * a.m[2][0]) * invDet;
    out.m[1][1] =  (a.m[0][0] * a.m[2][2] - a.m[0][2] * a.m[2][0]) * invDet;
    out.m[1][2] = -(a.m[0][0] * a.m[1][2] - a.m[0][2] * a.m[1][0]) * invDet;
    out.m[2][0] =  (a.m[1][0] * a.m[2][1] - a.m[1][1] * a.m[2][0]) * invDet;
    out.m[2][1] = -(a.m[0][0] * a.m[2][1] - a.m[0][1] * a.m[2][0]) * invDet;
    out.m[2][2] =  (a.m[0][0] * a.m[1][1] - a.m[0][1] * a.m[1][0]) * invDet;
    return out;
}

Vec2 applyAffineTransform(const Mat3& transform, const Vec2& point) {
    const float x = transform.m[0][0] * point.x + transform.m[0][1] * point.y + transform.m[0][2];
    const float y = transform.m[1][0] * point.x + transform.m[1][1] * point.y + transform.m[1][2];
    return Vec2{x, y};
}

Mat3 calculateAffineTransform(const Vec2Array& vertices, const std::vector<int>& triangle, const Vec2Array& deform) {
    const auto p0 = vertices.at(static_cast<std::size_t>(triangle[0]));
    const auto p1 = vertices.at(static_cast<std::size_t>(triangle[1]));
    const auto p2 = vertices.at(static_cast<std::size_t>(triangle[2]));
    Mat3 original{};
    original.m[0][0] = p0.x; original.m[0][1] = p1.x; original.m[0][2] = p2.x;
    original.m[1][0] = p0.y; original.m[1][1] = p1.y; original.m[1][2] = p2.y;
    original.m[2][0] = 1.0f; original.m[2][1] = 1.0f; original.m[2][2] = 1.0f;

    const auto d0 = deform.at(static_cast<std::size_t>(triangle[0]));
    const auto d1 = deform.at(static_cast<std::size_t>(triangle[1]));
    const auto d2 = deform.at(static_cast<std::size_t>(triangle[2]));
    const auto p3 = add(p0, d0);
    const auto p4 = add(p1, d1);
    const auto p5 = add(p2, d2);
    Mat3 transformed{};
    transformed.m[0][0] = p3.x; transformed.m[0][1] = p4.x; transformed.m[0][2] = p5.x;
    transformed.m[1][0] = p3.y; transformed.m[1][1] = p4.y; transformed.m[1][2] = p5.y;
    transformed.m[2][0] = 1.0f; transformed.m[2][1] = 1.0f; transformed.m[2][2] = 1.0f;

    return mulMat(transformed, invMat(original));
}
} // namespace

std::array<float, 3> barycentric(const ::nicxlive::core::nodes::Vec2& p,
                                 const ::nicxlive::core::nodes::Vec2& v0,
                                 const ::nicxlive::core::nodes::Vec2& v1,
                                 const ::nicxlive::core::nodes::Vec2& v2) {
    float denom = (v1.y - v2.y) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.y - v2.y);
    if (denom == 0.0f) return {-1.0f, -1.0f, -1.0f};
    float a = ((v1.y - v2.y) * (p.x - v2.x) + (v2.x - v1.x) * (p.y - v2.y)) / denom;
    float b = ((v2.y - v0.y) * (p.x - v2.x) + (v0.x - v2.x) * (p.y - v2.y)) / denom;
    float c = 1.0f - a - b;
    return {a, b, c};
}

bool pointInTriangle(const ::nicxlive::core::nodes::Vec2& p,
                     const ::nicxlive::core::nodes::Vec2& v0,
                     const ::nicxlive::core::nodes::Vec2& v1,
                     const ::nicxlive::core::nodes::Vec2& v2) {
    auto w = barycentric(p, v0, v1, v2);
    return w[0] >= 0.0f && w[1] >= 0.0f && w[2] >= 0.0f;
}

bool isPointInTriangle(const ::nicxlive::core::nodes::Vec2& pt, const ::nicxlive::core::nodes::Vec2Array& triangle) {
    if (triangle.size() < 3) return false;
    const auto p1 = triangle.at(0);
    const auto p2 = triangle.at(1);
    const auto p3 = triangle.at(2);
    const auto sign = [](const Vec2& a, const Vec2& b, const Vec2& c) {
        return (a.x - c.x) * (b.y - c.y) - (b.x - c.x) * (a.y - c.y);
    };
    const auto d1 = sign(pt, p1, p2);
    const auto d2 = sign(pt, p2, p3);
    const auto d3 = sign(pt, p3, p1);
    const bool hasNeg = (d1 < 0.0f) || (d2 < 0.0f) || (d3 < 0.0f);
    const bool hasPos = (d1 > 0.0f) || (d2 > 0.0f) || (d3 > 0.0f);
    return !(hasNeg && hasPos);
}

std::vector<int> findSurroundingTriangle(const ::nicxlive::core::nodes::Vec2& pt, ::nicxlive::core::nodes::MeshData& bindingMesh) {
    if (bindingMesh.indices.size() < 3) return {};
    std::vector<int> triangle{0, 1, 2};
    for (std::size_t i = 0; i + 2 < bindingMesh.indices.size(); i += 3) {
        triangle[0] = static_cast<int>(bindingMesh.indices[i]);
        triangle[1] = static_cast<int>(bindingMesh.indices[i + 1]);
        triangle[2] = static_cast<int>(bindingMesh.indices[i + 2]);
        if (triangle[0] < 0 || triangle[1] < 0 || triangle[2] < 0) continue;
        if (static_cast<std::size_t>(triangle[0]) >= bindingMesh.vertices.size()
            || static_cast<std::size_t>(triangle[1]) >= bindingMesh.vertices.size()
            || static_cast<std::size_t>(triangle[2]) >= bindingMesh.vertices.size()) {
            continue;
        }
        if (pointInTriangle(pt,
                            bindingMesh.vertices[static_cast<std::size_t>(triangle[0])],
                            bindingMesh.vertices[static_cast<std::size_t>(triangle[1])],
                            bindingMesh.vertices[static_cast<std::size_t>(triangle[2])])) {
            return triangle;
        }
    }
    return {};
}

::nicxlive::core::nodes::Vec2 calcOffsetInTriangleCoords(const ::nicxlive::core::nodes::Vec2& pt,
                                                          ::nicxlive::core::nodes::MeshData& bindingMesh,
                                                          std::vector<int>& triangle) {
    if (triangle.size() < 3) return Vec2{0.0f, 0.0f};
    auto dist2 = [&](int idx) {
        const auto& v = bindingMesh.vertices[static_cast<std::size_t>(idx)];
        const auto dx = pt.x - v.x;
        const auto dy = pt.y - v.y;
        return dx * dx + dy * dy;
    };
    if (dist2(triangle[0]) > dist2(triangle[1])) std::swap(triangle[0], triangle[1]);
    if (dist2(triangle[0]) > dist2(triangle[2])) std::swap(triangle[0], triangle[2]);

    const auto p1 = bindingMesh.vertices[static_cast<std::size_t>(triangle[0])];
    const auto p2 = bindingMesh.vertices[static_cast<std::size_t>(triangle[1])];
    const auto p3 = bindingMesh.vertices[static_cast<std::size_t>(triangle[2])];
    auto axis0 = sub(p2, p1);
    const auto axis0len = length(axis0);
    if (axis0len <= 1e-8f) return Vec2{0.0f, 0.0f};
    axis0 = mul(axis0, 1.0f / axis0len);

    auto axis1 = sub(p3, p1);
    const auto axis1len = length(axis1);
    if (axis1len <= 1e-8f) return Vec2{0.0f, 0.0f};
    axis1 = mul(axis1, 1.0f / axis1len);

    const auto relPt = sub(pt, p1);
    if (dot(relPt, relPt) <= 1e-8f) return Vec2{0.0f, 0.0f};

    const auto cosA = dot(axis0, axis1);
    if (std::abs(cosA) <= 1e-8f) {
        return Vec2{dot(relPt, axis0), dot(relPt, axis1)};
    }

    const auto argA = std::acos(std::clamp(cosA, -1.0f, 1.0f));
    const auto sinA = std::sin(argA);
    const auto tanA = std::tan(argA);
    if (std::abs(sinA) <= 1e-8f || std::abs(tanA) <= 1e-8f) {
        return Vec2{dot(relPt, axis0), dot(relPt, axis1)};
    }

    const auto relLen = length(relPt);
    const auto cosB = dot(axis0, relPt) / relLen;
    const auto argB = std::acos(std::clamp(cosB, -1.0f, 1.0f));
    const auto sinB = std::sin(argB);
    const Vec2 ortPt{relLen * cosB, relLen * sinB};
    const float x = ortPt.x - (1.0f / tanA) * ortPt.y;
    const float y = (1.0f / sinA) * ortPt.y;
    return Vec2{x, y};
}

bool nlCalculateTransformInTriangle(const ::nicxlive::core::nodes::Vec2Array& vertices,
                                    const std::vector<int>& triangle,
                                    const ::nicxlive::core::nodes::Vec2Array& deform,
                                    const ::nicxlive::core::nodes::Vec2& target,
                                    ::nicxlive::core::nodes::Vec2& targetPrime,
                                    float& rotVert,
                                    float& rotHorz) {
    if (triangle.size() < 3) return false;
    if (vertices.size() == 0 || deform.size() == 0) return false;
    const auto affineTransform = calculateAffineTransform(vertices, triangle, deform);
    targetPrime = applyAffineTransform(affineTransform, target);

    const Vec2 targetVert = add(target, Vec2{0.0f, 1.0f});
    const Vec2 targetVertPrime = applyAffineTransform(affineTransform, targetVert);
    const Vec2 targetHorz = add(target, Vec2{1.0f, 0.0f});
    const Vec2 targetHorzPrime = applyAffineTransform(affineTransform, targetHorz);

    const float originalAngleVert = calculateAngle(target, targetVert);
    const float transformedAngleVert = calculateAngle(targetPrime, targetVertPrime);
    rotVert = transformedAngleVert - originalAngleVert;

    const float originalAngleHorz = calculateAngle(target, targetHorz);
    const float transformedAngleHorz = calculateAngle(targetPrime, targetHorzPrime);
    rotHorz = transformedAngleHorz - originalAngleHorz;
    return true;
}

} // namespace nicxlive::core::math
