// Independent nijilive.math.triangle used-source adaptation.
// Copyright (c) 2020, Inochi2D Project. BSD-2-Clause: ../../LICENSE.
// Inmath inverse: David Herberth/Inochi2D, MIT: ../animation/inmath.LICENSE.
#include "triangle.hpp"
#include "../nodes/drawable.hpp"

#include <algorithm>
#include <cmath>
#include <bit>
#include <limits>
#include "inmath_float.hpp"
#include "../compat/native_access_fault.hpp"
#include "../compat/static_error.hpp"

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

// Veca indexing forms its proxy before the proxy reads the component lanes.
Vec2 sourceTrianglePoint(const Vec2Array& values, std::size_t index, bool constIndex = false) {
#ifndef NDEBUG
    if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::AssertError>(
        "veca index out of range", "source/nijilive/math/veca.d", constIndex ? 144 : 139);
#endif
    if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
        index, values.size(), d_runtime::RetainedString::fromStaticLiteral("source/nijilive/math/veca.d"), 467);
    return {values.xAt(index), values.yAt(index)};
}

int& sourceTriangleIndex(common::SharedArray<int>& values, std::size_t index, std::size_t line) {
    if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
        index, values.size(), d_runtime::RetainedString::fromStaticLiteral(
            "source/nijilive/math/triangle.d"), line);
    return values[index];
}

long double sourceTriangleLengthSquared(Vec2 value) {
    long double result = 0;
    result += static_cast<long double>(value.x * value.x);
    result += static_cast<long double>(value.y * value.y);
    return result;
}

long double sourceTriangleLength(Vec2 value) {
    return std::sqrt(sourceTriangleLengthSquared(value));
}

float sourceTriangleDot(Vec2 left, Vec2 right) {
    float result = 0;
    result = detail::inmathAccumulate(result, detail::inmathProduct(left.x, right.x));
    return detail::inmathAccumulate(result, detail::inmathProduct(left.y, right.y));
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
                out.m[i][j] = detail::inmathAccumulate(out.m[i][j],
                    detail::inmathProduct(a.m[i][k], b.m[k][j]));
            }
        }
    }
    return out;
}

// inmath 1.0.6 linalg.d1191-1221; MIT, animation/inmath.LICENSE.
float detMat(const Mat3& a) {
    return a.m[0][0] * a.m[1][1] * a.m[2][2]
         + a.m[0][1] * a.m[1][2] * a.m[2][0]
         + a.m[0][2] * a.m[1][0] * a.m[2][1]
         - a.m[0][2] * a.m[1][1] * a.m[2][0]
         - a.m[0][1] * a.m[1][0] * a.m[2][2]
         - a.m[0][0] * a.m[1][2] * a.m[2][1];
}

Mat3 invMat(const Mat3& a) {
    Mat3 out{};
    const float d = 1.0f / detMat(a);
    out.m[0][0] = (a.m[1][1] * a.m[2][2] - a.m[1][2] * a.m[2][1]) * d;
    out.m[0][1] = (a.m[0][2] * a.m[2][1] - a.m[0][1] * a.m[2][2]) * d;
    out.m[0][2] = (a.m[0][1] * a.m[1][2] - a.m[0][2] * a.m[1][1]) * d;
    out.m[1][0] = (a.m[1][2] * a.m[2][0] - a.m[1][0] * a.m[2][2]) * d;
    out.m[1][1] = (a.m[0][0] * a.m[2][2] - a.m[0][2] * a.m[2][0]) * d;
    out.m[1][2] = (a.m[0][2] * a.m[1][0] - a.m[0][0] * a.m[1][2]) * d;
    out.m[2][0] = (a.m[1][0] * a.m[2][1] - a.m[1][1] * a.m[2][0]) * d;
    out.m[2][1] = (a.m[0][1] * a.m[2][0] - a.m[0][0] * a.m[2][1]) * d;
    out.m[2][2] = (a.m[0][0] * a.m[1][1] - a.m[0][1] * a.m[1][0]) * d;
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

bool ngIsPointInTriangleSource(Vec2 point, const Vec2Array triangle) {
#ifndef NDEBUG
    if (triangle.size() < 3) throw d_runtime::ngStaticError<d_runtime::AssertError>(
        "Triangle needs at least 3 vertices", "source/nijilive/math/triangle.d", 13);
#endif
    const Vec2 p1 = sourceTrianglePoint(triangle, 0, true);
    const Vec2 p2 = sourceTrianglePoint(triangle, 1, true);
    const Vec2 p3 = sourceTrianglePoint(triangle, 2, true);
    const auto sign = [](Vec2 a, Vec2 b, Vec2 c) {
        return (a.x - c.x) * (b.y - c.y) - (b.x - c.x) * (a.y - c.y);
    };
    const float d1 = sign(point, p1, p2);
    const float d2 = sign(point, p2, p3);
    const float d3 = sign(point, p3, p1);
    return !((d1 < 0 || d2 < 0 || d3 < 0) && (d1 > 0 || d2 > 0 || d3 > 0));
}

Vec2 ngCalcOffsetInTriangleCoordsSource(Vec2 point, MeshData& mesh,
        common::SharedArray<int>& triangle) {
    const auto vertex = [&](std::size_t item, std::size_t line) {
        const int index = sourceTriangleIndex(triangle, item, line);
        return sourceTrianglePoint(mesh.vertices, static_cast<std::size_t>(index));
    };
    const auto distance = [&](std::size_t item, std::size_t line) {
        return sourceTriangleLengthSquared(sub(point, vertex(item, line)));
    };
    // The second comparison observes the first swap through the actual descriptor.
    const auto firstDistance = distance(0, 67);
    const auto secondDistance = distance(1, 67);
    if (firstDistance > secondDistance) {
        auto& first = sourceTriangleIndex(triangle, 0, 68);
        auto& second = sourceTriangleIndex(triangle, 1, 68);
        std::swap(first, second);
    }
    const auto currentDistance = distance(0, 70);
    const auto thirdDistance = distance(2, 70);
    if (currentDistance > thirdDistance) {
        auto& first = sourceTriangleIndex(triangle, 0, 71);
        auto& third = sourceTriangleIndex(triangle, 2, 71);
        std::swap(first, third);
    }
    const Vec2 p1 = vertex(0, 73);
    const Vec2 p2 = vertex(1, 74);
    const Vec2 p3 = vertex(2, 75);
    Vec2 axis0 = sub(p2, p1);
    [[maybe_unused]] const float axis0Length = static_cast<float>(sourceTriangleLength(axis0));
    const float divisor0 = static_cast<float>(sourceTriangleLength(axis0));
    axis0 = {axis0.x / divisor0, axis0.y / divisor0};
    Vec2 axis1 = sub(p3, p1);
    [[maybe_unused]] const float axis1Length = static_cast<float>(sourceTriangleLength(axis1));
    const float divisor1 = static_cast<float>(sourceTriangleLength(axis1));
    axis1 = {axis1.x / divisor1, axis1.y / divisor1};
    const Vec2 relative = sub(point, p1);
    if (sourceTriangleLengthSquared(relative) == 0) return {0, 0};
    const float cosA = sourceTriangleDot(axis0, axis1);
    if (cosA == 0) return {sourceTriangleDot(relative, axis0), sourceTriangleDot(relative, axis1)};
    // Selected std.math acos(float) promotes to Source real, then stores float.
    const float argA = static_cast<float>(std::acos(static_cast<long double>(cosA)));
    const float sinA = std::sin(argA);
    const float tanA = std::tan(argA);
    const float cosB = static_cast<float>(sourceTriangleDot(axis0, relative) / sourceTriangleLength(relative));
    const float argB = static_cast<float>(std::acos(static_cast<long double>(cosB)));
    const float sinB = std::sin(argB);
    const Vec2 orthogonal{
        static_cast<float>(sourceTriangleLength(relative) * cosB),
        static_cast<float>(sourceTriangleLength(relative) * sinB)};
    // inmath dynamic-array constructor is row-major; multiplication folds columns.
    const float matrix[2][2]{{1, -1.0f / tanA}, {0, 1.0f / sinA}};
    const float input[2]{orthogonal.x, orthogonal.y};
    float output[2]{0, 0};
    for (std::size_t column = 0; column < 2; ++column)
        for (std::size_t row = 0; row < 2; ++row)
            output[row] = detail::inmathAccumulate(output[row],
                detail::inmathProduct(matrix[row][column], input[column]));
    return {output[0], output[1]};
}

static common::SharedArray<int> findSourceTriangle(Vec2 point, MeshData* mesh) {
    common::SharedArray<int> triangle(std::vector<int>{0, 1, 2});
    const auto checked = [](const auto& array, std::size_t index, std::size_t line) -> decltype(auto) {
        if (index >= array.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
            index, array.size(), d_runtime::RetainedString::fromStaticLiteral(
                "source/nijilive/math/triangle.d"), line);
        return array[index];
    };
    std::uint32_t position = 0;
    while (static_cast<std::size_t>(std::bit_cast<std::int32_t>(position)) <
            d_runtime::ngRequireObject(mesh)->indices.size()) {
        const auto first = static_cast<std::size_t>(std::bit_cast<std::int32_t>(position));
        const auto second = static_cast<std::size_t>(std::bit_cast<std::int32_t>(position + 1));
        const auto third = static_cast<std::size_t>(std::bit_cast<std::int32_t>(position + 2));
        triangle[0] = checked(mesh->indices, first, 53);
        triangle[1] = checked(mesh->indices, second, 54);
        triangle[2] = checked(mesh->indices, third, 55);
        const Vec2 p1 = sourceTrianglePoint(mesh->vertices, static_cast<std::size_t>(triangle[0]));
        const Vec2 p2 = sourceTrianglePoint(mesh->vertices, static_cast<std::size_t>(triangle[1]));
        const Vec2 p3 = sourceTrianglePoint(mesh->vertices, static_cast<std::size_t>(triangle[2]));
        const auto sign = [](Vec2 a, Vec2 b, Vec2 c) {
            return (a.x - c.x) * (b.y - c.y) - (b.x - c.x) * (a.y - c.y);
        };
        const auto d1 = sign(point, p1, p2), d2 = sign(point, p2, p3), d3 = sign(point, p3, p1);
        const bool negative = d1 < 0 || d2 < 0 || d3 < 0;
        const bool positive = d1 > 0 || d2 > 0 || d3 > 0;
        if (!(negative && positive)) return triangle;
        position += 3;
    }
    return {};
}

common::SharedArray<int> ngFindSurroundingTriangleSource(Vec2 point, MeshData& mesh) {
    return findSourceTriangle(point, &mesh);
}

common::SharedArray<int> findSurroundingTriangle(Vec2 point, std::shared_ptr<MeshData> mesh) {
    return findSourceTriangle(point, mesh.get());
}

bool nlCalculateTransformInTriangle(Vec2Array vertices, common::SharedArray<int> triangle,
        Vec2Array deformation, Vec2 target, Vec2& targetPrime, float& rotVert, float& rotHorz) {
    // Source out arguments are initialized on entry, before any failing body access.
    const float initial = std::numeric_limits<float>::quiet_NaN();
    targetPrime = {initial, initial};
    rotVert = rotHorz = initial;
    const auto checked = [](const auto& array, std::size_t index, std::size_t line) -> decltype(auto) {
        if (index >= array.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
            index, array.size(), d_runtime::RetainedString::fromStaticLiteral(
                "source/nijilive/math/triangle.d"), line);
        return array[index];
    };
    const auto point = [&](const Vec2Array& values, std::size_t item, std::size_t line) -> Vec2 {
        const auto index = checked(triangle, item, line);
        return sourceTrianglePoint(values, static_cast<std::size_t>(index));
    };
    const Vec2 p0 = point(vertices, 0, 108), p1 = point(vertices, 1, 109), p2 = point(vertices, 2, 110);
    Mat3 original{};
    original.m[0][0] = p0.x; original.m[0][1] = p1.x; original.m[0][2] = p2.x;
    original.m[1][0] = p0.y; original.m[1][1] = p1.y; original.m[1][2] = p2.y;
    original.m[2][0] = original.m[2][1] = original.m[2][2] = 1;
    const Vec2 p3 = add(p0, point(deformation, 0, 116));
    const Vec2 p4 = add(p1, point(deformation, 1, 117));
    const Vec2 p5 = add(p2, point(deformation, 2, 118));
    Mat3 transformed{};
    transformed.m[0][0] = p3.x; transformed.m[0][1] = p4.x; transformed.m[0][2] = p5.x;
    transformed.m[1][0] = p3.y; transformed.m[1][1] = p4.y; transformed.m[1][2] = p5.y;
    transformed.m[2][0] = transformed.m[2][1] = transformed.m[2][2] = 1;
    const auto affine = mulMat(transformed, invMat(original));
    const auto apply = [&](Vec2 value) {
        const float input[]{value.x, value.y, 1};
        Vec2 result{};
        float* components[]{&result.x, &result.y};
        for (std::size_t column = 0; column < 3; ++column)
            for (std::size_t row = 0; row < 2; ++row)
                *components[row] = detail::inmathAccumulate(*components[row],
                    detail::inmathProduct(affine.m[row][column], input[column]));
        return result;
    };
    targetPrime = apply(target);
    const Vec2 vertical = add(target, Vec2{0, 1});
    const Vec2 verticalPrime = apply(vertical);
    const Vec2 horizontal = add(target, Vec2{1, 0});
    const Vec2 horizontalPrime = apply(horizontal);
    const float originalVertical = calculateAngle(target, vertical);
    const float transformedVertical = calculateAngle(targetPrime, verticalPrime);
    rotVert = transformedVertical - originalVertical;
    const float originalHorizontal = calculateAngle(target, horizontal);
    const float transformedHorizontal = calculateAngle(targetPrime, horizontalPrime);
    rotHorz = transformedHorizontal - originalHorizontal;
    return true;
}

} // namespace nicxlive::core::math
