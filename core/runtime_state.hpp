#pragma once

#include "math/camera.hpp"
#include "render/common.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace nicxlive::core {

// Viewport stack
void inPushViewport(int width, int height);
void inPopViewport();
void inSetViewport(int width, int height);
void inGetViewport(int& width, int& height);
void inEnsureViewportForTests(int width = 640, int height = 480);
std::size_t inViewportDataLength();
void inDumpViewport(std::vector<uint8_t>& dumpTo);

// Camera stack
void inPushCamera();
void inPushCamera(const math::Camera& camera);
void inPopCamera();
math::Camera& inGetCamera();
void inSetCamera(const math::Camera& camera);
void inEnsureCameraStackForTests();

// Clear color
void inSetClearColor(float r, float g, float b, float a);
void inGetClearColor(float& r, float& g, float& b, float& a);

// Backend accessors
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
