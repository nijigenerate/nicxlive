#pragma once

#include "math/camera.hpp"
#include "render/common.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace nicxlive::core {

// Original package-visible TLS storage. All operations below use these actual descriptors.
extern thread_local common::SharedArray<int> inViewportWidth;
extern thread_local common::SharedArray<int> inViewportHeight;
using CameraStack = common::SharedArray<std::shared_ptr<math::Camera>>;
extern thread_local CameraStack inCamera;
extern thread_local math::Vec4 inClearColor;

// Viewport stack
void inPushViewport(int width, int height);
void inPopViewport();
void inSetViewport(int width, int height);
void inGetViewport(int& width, int& height);
void inEnsureViewportForTests(int width = 640, int height = 480);
std::size_t inViewportDataLength();
void inDumpViewport(std::vector<uint8_t>& dumpTo);
void inDumpViewport(common::SharedArray<std::uint8_t>& dumpTo);

// Camera stack
void inPushCamera();
void inPushCamera(const math::Camera& camera);
void inPushCamera(std::shared_ptr<math::Camera> camera);
void inPopCamera();
// Existing native reference adapter; Source class handles use the next API.
math::Camera& inGetCamera();
// Canonical Source class result: retaining, nullable and returned by value.
std::shared_ptr<math::Camera> inGetCameraHandle();
void inSetCamera(const math::Camera& camera);
void inSetCamera(std::shared_ptr<math::Camera> camera);
void inEnsureCameraStackForTests();

// The original vec3.vector is an actual mutable contiguous float[3].
// Keep that storage for the UI pointer; shader calls copy it into Vec3.
extern thread_local std::array<float, 3> inSceneAmbientLight;

// Clear color
void inSetClearColor(float r, float g, float b, float a);
void inGetClearColor(float& r, float& g, float& b, float& a);

// Backend accessors
using RenderBackendFactory = std::shared_ptr<RenderBackend> (*)();
void ngSetRenderBackendFactory(RenderBackendFactory factory);
void inSetRenderBackend(const std::shared_ptr<RenderBackend>& backend);
std::shared_ptr<RenderBackend> tryRenderBackend();
std::shared_ptr<RenderBackend> requireRenderBackend();
std::shared_ptr<RenderBackend> currentRenderBackend();

// Render targets
RenderResourceHandle inGetRenderImage();
RenderResourceHandle inGetFramebuffer();
RenderResourceHandle inGetCompositeImage();
RenderResourceHandle inGetCompositeFramebuffer();
RenderResourceHandle inGetMainAlbedo();
RenderResourceHandle inGetMainEmissive();
RenderResourceHandle inGetMainBump();
RenderResourceHandle inGetCompositeEmissive();
RenderResourceHandle inGetCompositeBump();
RenderResourceHandle inGetBlendFramebuffer();
RenderResourceHandle inGetBlendAlbedo();
RenderResourceHandle inGetBlendEmissive();
RenderResourceHandle inGetBlendBump();

// Scene hooks
void inBeginScene();
void inEndScene();
void inPostProcessScene();
void inPostProcessingAddBasicLighting();

// Renderer init (D: initRendererCommon/initRenderer)
void initRendererCommon();
void initRenderer();

// Difference aggregation
void inSetDifferenceAggregationEnabled(bool enabled);
bool inIsDifferenceAggregationEnabled();
void inSetDifferenceAggregationRegion(int x, int y, int width, int height);
DifferenceEvaluationRegion inGetDifferenceAggregationRegion();
bool inEvaluateDifferenceAggregation(RenderResourceHandle texture, int viewportWidth, int viewportHeight);
bool inFetchDifferenceAggregationResult(DifferenceEvaluationResult& result);

} // namespace nicxlive::core
