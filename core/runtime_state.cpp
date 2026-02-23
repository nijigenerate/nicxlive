#include "runtime_state.hpp"

#include "render/common.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace nicxlive::core {

namespace {
std::vector<int> gViewportWidth{};
std::vector<int> gViewportHeight{};
std::vector<math::Camera> gCameras{};
nodes::Vec4 gClearColor{0.0f, 0.0f, 0.0f, 0.0f};
std::shared_ptr<RenderBackend> gCachedRenderBackend{};
} // namespace

void inSetRenderBackend(const std::shared_ptr<RenderBackend>& backend) {
    gCachedRenderBackend = backend;
    setCurrentRenderBackend(backend);
}

std::shared_ptr<RenderBackend> tryRenderBackend() {
    if (!gCachedRenderBackend) {
        gCachedRenderBackend = getCurrentRenderBackend();
    }
    if (!gCachedRenderBackend) {
        gCachedRenderBackend = std::make_shared<RenderBackend>();
        setCurrentRenderBackend(gCachedRenderBackend);
    }
    return gCachedRenderBackend;
}

std::shared_ptr<RenderBackend> requireRenderBackend() {
    auto backend = tryRenderBackend();
    if (!backend) {
        throw std::runtime_error("RenderBackend is not available.");
    }
    return backend;
}

std::shared_ptr<RenderBackend> currentRenderBackend() {
    return requireRenderBackend();
}

void inPushViewport(int width, int height) {
    gViewportWidth.push_back(width);
    gViewportHeight.push_back(height);
    inPushCamera();
}

void inPopViewport() {
    if (gViewportWidth.size() > 1) {
        gViewportWidth.pop_back();
        gViewportHeight.pop_back();
        inPopCamera();
    }
}

void inSetViewport(int width, int height) {
    if (gViewportWidth.empty()) {
        inPushViewport(width, height);
    } else {
        if (gViewportWidth.back() == width && gViewportHeight.back() == height) {
            requireRenderBackend()->resizeViewportTargets(width, height);
            return;
        }
        gViewportWidth.back() = width;
        gViewportHeight.back() = height;
    }
    requireRenderBackend()->resizeViewportTargets(width, height);
}

void inGetViewport(int& width, int& height) {
    if (gViewportWidth.empty()) {
        width = 0;
        height = 0;
        return;
    }
    width = gViewportWidth.back();
    height = gViewportHeight.back();
}

void inEnsureViewportForTests(int width, int height) {
    if (gViewportWidth.empty()) {
        inPushViewport(width, height);
    }
}

std::size_t inViewportDataLength() {
    if (gViewportWidth.empty()) return 0;
    return static_cast<std::size_t>(gViewportWidth.back()) * static_cast<std::size_t>(gViewportHeight.back()) * 4;
}

void inDumpViewport(std::vector<uint8_t>& dumpTo) {
    const int width = gViewportWidth.empty() ? 0 : gViewportWidth.back();
    const int height = gViewportHeight.empty() ? 0 : gViewportHeight.back();
    const auto required = static_cast<std::size_t>(std::max(0, width)) * static_cast<std::size_t>(std::max(0, height)) * 4;
    if (dumpTo.size() < required) {
        throw std::runtime_error("Invalid data destination length for inDumpViewport");
    }

    if (auto backend = tryRenderBackend()) {
        backend->dumpViewport(dumpTo, width, height);
    } else {
        std::fill_n(dumpTo.begin(), static_cast<std::ptrdiff_t>(required), static_cast<uint8_t>(0));
    }

    if (width <= 0 || height <= 0) return;
    std::vector<uint8_t> tmpLine(static_cast<std::size_t>(width) * 4);
    std::size_t ri = 0;
    for (int i = height - 1; i >= (height / 2); --i) {
        const std::size_t lineSize = static_cast<std::size_t>(width) * 4;
        const std::size_t oldLineStart = lineSize * ri;
        const std::size_t newLineStart = lineSize * static_cast<std::size_t>(i);
        std::copy_n(dumpTo.begin() + static_cast<std::ptrdiff_t>(oldLineStart), static_cast<std::ptrdiff_t>(lineSize), tmpLine.begin());
        std::copy_n(dumpTo.begin() + static_cast<std::ptrdiff_t>(newLineStart), static_cast<std::ptrdiff_t>(lineSize),
                    dumpTo.begin() + static_cast<std::ptrdiff_t>(oldLineStart));
        std::copy_n(tmpLine.begin(), static_cast<std::ptrdiff_t>(lineSize), dumpTo.begin() + static_cast<std::ptrdiff_t>(newLineStart));
        ++ri;
    }
}

void inPushCamera() {
    gCameras.emplace_back();
}

void inPushCamera(const math::Camera& camera) {
    gCameras.push_back(camera);
}

void inPopCamera() {
    if (gCameras.size() > 1) {
        gCameras.pop_back();
    }
}

math::Camera& inGetCamera() {
    if (gCameras.empty()) {
        gCameras.emplace_back();
    }
    return gCameras.back();
}

void inSetCamera(const math::Camera& camera) {
    if (gCameras.empty()) {
        gCameras.push_back(camera);
    } else {
        gCameras.back() = camera;
    }
}

void inEnsureCameraStackForTests() {
    if (gCameras.empty()) {
        gCameras.emplace_back();
    }
}

void inSetClearColor(float r, float g, float b, float a) {
    gClearColor = nodes::Vec4{r, g, b, a};
}

void inGetClearColor(float& r, float& g, float& b, float& a) {
    r = gClearColor.x;
    g = gClearColor.y;
    b = gClearColor.z;
    a = gClearColor.w;
}

RenderResourceHandle inGetRenderImage() {
    auto backend = tryRenderBackend();
    return backend ? backend->renderImageHandle() : 0;
}

RenderResourceHandle inGetFramebuffer() {
    auto backend = tryRenderBackend();
    return backend ? backend->framebufferHandle() : 0;
}

RenderResourceHandle inGetCompositeImage() { return 0; }
RenderResourceHandle inGetCompositeFramebuffer() { return 0; }

RenderResourceHandle inGetMainAlbedo() {
    auto backend = tryRenderBackend();
    return backend ? backend->mainAlbedoHandle() : 0;
}

RenderResourceHandle inGetMainEmissive() {
    auto backend = tryRenderBackend();
    return backend ? backend->mainEmissiveHandle() : 0;
}

RenderResourceHandle inGetMainBump() {
    auto backend = tryRenderBackend();
    return backend ? backend->mainBumpHandle() : 0;
}

RenderResourceHandle inGetCompositeEmissive() { return 0; }
RenderResourceHandle inGetCompositeBump() { return 0; }

RenderResourceHandle inGetBlendFramebuffer() {
    auto backend = tryRenderBackend();
    return backend ? backend->blendFramebufferHandle() : 0;
}

RenderResourceHandle inGetBlendAlbedo() {
    auto backend = tryRenderBackend();
    return backend ? backend->blendAlbedoHandle() : 0;
}

RenderResourceHandle inGetBlendEmissive() {
    auto backend = tryRenderBackend();
    return backend ? backend->blendEmissiveHandle() : 0;
}

RenderResourceHandle inGetBlendBump() {
    auto backend = tryRenderBackend();
    return backend ? backend->blendBumpHandle() : 0;
}

void inBeginScene() {
    if (auto backend = tryRenderBackend()) backend->beginScene();
}

void inEndScene() {
    if (auto backend = tryRenderBackend()) backend->endScene();
}

void inPostProcessScene() {
    if (auto backend = tryRenderBackend()) backend->postProcessScene();
}

void inPostProcessingAddBasicLighting() {
    if (auto backend = tryRenderBackend()) backend->addBasicLightingPostProcess();
}

void initRendererCommon() {
    inPushViewport(0, 0);
    inSetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
}

void initRenderer() {
    initRendererCommon();
    requireRenderBackend()->initializeRenderer();
}

void inSetDifferenceAggregationEnabled(bool enabled) {
    if (auto backend = tryRenderBackend()) backend->setDifferenceAggregationEnabled(enabled);
}

bool inIsDifferenceAggregationEnabled() {
    if (auto backend = tryRenderBackend()) return backend->isDifferenceAggregationEnabled();
    return false;
}

void inSetDifferenceAggregationRegion(int x, int y, int width, int height) {
    if (auto backend = tryRenderBackend()) {
        backend->setDifferenceAggregationRegion(DifferenceEvaluationRegion{x, y, width, height});
    }
}

DifferenceEvaluationRegion inGetDifferenceAggregationRegion() {
    if (auto backend = tryRenderBackend()) return backend->getDifferenceAggregationRegion();
    return DifferenceEvaluationRegion{};
}

bool inEvaluateDifferenceAggregation(RenderResourceHandle texture, int viewportWidth, int viewportHeight) {
    if (auto backend = tryRenderBackend()) {
        return backend->evaluateDifferenceAggregation(texture, viewportWidth, viewportHeight);
    }
    return false;
}

bool inFetchDifferenceAggregationResult(DifferenceEvaluationResult& result) {
    if (auto backend = tryRenderBackend()) {
        return backend->fetchDifferenceAggregationResult(result);
    }
    result = DifferenceEvaluationResult{};
    return false;
}

} // namespace nicxlive::core
