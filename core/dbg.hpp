#pragma once
// Canonical nijilive.core.dbg runtime and existing native provider intake.
#include "math/veca.hpp"
#include "math/mat4.hpp"
#include "render/common.hpp"
#include "common/shared_array.hpp"
#include <cstdint>
#include <memory>
#include <span>

namespace nicxlive::core::dbg {
using nicxlive::core::math::Vec3;
using nicxlive::core::math::Vec4;
using nicxlive::core::math::Vec3Array;
using nicxlive::core::math::Mat4;
using nicxlive::core::RenderResourceHandle;

extern thread_local bool inDbgDrawMeshOutlines;
extern thread_local bool inDbgDrawMeshVertexPoints;
extern thread_local bool inDbgDrawMeshOrientation;

// Native rendering lifecycle selects a concrete provider; no backend is synthesized.
class DebugProvider {
public:
    virtual ~DebugProvider() = default;
    virtual void initialize() = 0;
    virtual void pointSize(float size) = 0;
    virtual void lineWidth(float size) = 0;
    virtual void upload(const Vec3Array& points, std::span<const std::uint16_t> indices) = 0;
    virtual void external(RenderResourceHandle vbo, RenderResourceHandle ibo, int count) = 0;
    virtual void points(Vec4 color, const Mat4& mvp) = 0;
    virtual void lines(Vec4 color, const Mat4& mvp) = 0;
};
void ngInstallDebugProvider(std::shared_ptr<DebugProvider> provider,
    std::shared_ptr<nicxlive::core::RenderBackend> runtimeBackend);
void ngClearDebugProvider();
void inInitDebug();
void inDbgPointsSize(float size);
void inDbgLineWidth(float size);
void inDbgSetBuffer(const Vec3Array& points);
void inDbgSetBuffer(RenderResourceHandle vbo, RenderResourceHandle ibo, int count);
void inDbgSetBuffer(const Vec3Array& points, std::span<const std::uint16_t> indices);
// Original array value arguments retain their actual backing through callbacks.
void inDbgSetBuffer(Vec3Array points, common::SharedArray<std::uint16_t> indices);
void inDbgSetBuffer(std::span<const Vec3> points);
void inDbgDrawPoints(Vec4 color, const Mat4& transform = Mat4::identity());
void inDbgDrawLines(Vec4 color, const Mat4& transform = Mat4::identity());

}
