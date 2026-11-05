#include "runtime_state.hpp"
#include "nodes/initialization.hpp"
#include "diff_collect.hpp"

#include "render/common.hpp"
#include "texture.hpp"
#include "param/parameter.hpp"
#include "compat/array_range_error.hpp"
#include "compat/throwable.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace nicxlive::core {

namespace {
std::shared_ptr<RenderBackend> gCachedRenderBackend{};
RenderBackendFactory gRenderBackendFactory = nullptr;

// D int arithmetic wraps before conversion to the returned size_t.
std::int32_t viewportProduct(int width, int height, std::uint32_t channels) {
    static_assert(sizeof(int) == sizeof(std::int32_t));
    return std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(width) * static_cast<std::uint32_t>(height) * channels);
}

template<class Array> decltype(auto) runtimeLast(Array& values, std::size_t line) {
    if (values.empty()) {
        throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
            values.size() - 1, values.size(),
            d_runtime::RetainedString::fromStaticLiteral("nijilive/core/runtime_state.d"), line);
    }
    return values[values.size() - 1];
}
} // namespace

thread_local common::SharedArray<int> inViewportWidth;
thread_local common::SharedArray<int> inViewportHeight;
thread_local CameraStack inCamera;
thread_local math::Vec4 inClearColor{0.0f, 0.0f, 0.0f, 0.0f};
thread_local std::array<float, 3> inSceneAmbientLight{1.0f, 1.0f, 1.0f};

void inSetRenderBackend(const std::shared_ptr<RenderBackend>& backend) {
    // Detach is the existing end-of-renderer lifecycle boundary. Drain while the
    // old backend and its current graphics context are still available.
    if (!backend) inDrainPendingTextureDisposals();
    gCachedRenderBackend = backend;
    setCurrentRenderBackend(backend);
}

void ngSetRenderBackendFactory(RenderBackendFactory factory) { gRenderBackendFactory = factory; }

std::shared_ptr<RenderBackend> tryRenderBackend() {
    if (!gCachedRenderBackend) {
        gCachedRenderBackend = getCurrentRenderBackend();
    }
    if (!gCachedRenderBackend) {
        const auto factory = gRenderBackendFactory;
        if (!factory) throw d_runtime::ThrowableException(
            d_runtime::RetainedString::fromStaticLiteral("RenderBackend is not available."),
            d_runtime::RetainedString::fromStaticLiteral("nijilive/core/runtime_state.d"), 187);
        auto backend = factory();
        if (!backend) throw d_runtime::ThrowableException(
            d_runtime::RetainedString::fromStaticLiteral("RenderBackend is not available."),
            d_runtime::RetainedString::fromStaticLiteral("nijilive/core/runtime_state.d"), 187);
        gCachedRenderBackend = backend;
        setCurrentRenderBackend(backend);
    }
    return gCachedRenderBackend;
}

std::shared_ptr<RenderBackend> requireRenderBackend() {
    auto backend = tryRenderBackend();
    if (!backend) {
        throw d_runtime::ThrowableException(
            d_runtime::RetainedString::fromStaticLiteral("RenderBackend is not available."),
            d_runtime::RetainedString::fromStaticLiteral("nijilive/core/runtime_state.d"), 187);
    }
    return backend;
}

std::shared_ptr<RenderBackend> currentRenderBackend() {
    return requireRenderBackend();
}

void inPushViewport(int width, int height) {
    inViewportWidth.push_back(width);
    inViewportHeight.push_back(height);
    inPushCamera();
}

void inPopViewport() {
    if (inViewportWidth.size() > 1) {
        inViewportWidth.resize(inViewportWidth.size() - 1);
        inViewportHeight.resize(inViewportHeight.size() - 1);
        inPopCamera();
    }
}

void inSetViewport(int width, int height) {
    if (inViewportWidth.empty()) {
        inPushViewport(width, height);
    } else {
        if (runtimeLast(inViewportWidth, 107) == width && runtimeLast(inViewportHeight, 107) == height) {
            requireRenderBackend()->resizeViewportTargets(width, height);
            return;
        }
        runtimeLast(inViewportWidth, 111) = width;
        runtimeLast(inViewportHeight, 112) = height;
    }
    requireRenderBackend()->resizeViewportTargets(width, height);
}

void inGetViewport(int& width, int& height) {
    width = 0;
    height = 0;
    if (inViewportWidth.empty()) {
        width = 0;
        height = 0;
        return;
    }
    width = runtimeLast(inViewportWidth, 126);
    height = runtimeLast(inViewportHeight, 127);
}

void inEnsureViewportForTests(int width, int height) {
    if (inViewportWidth.empty()) {
        inPushViewport(width, height);
    }
}

std::size_t inViewportDataLength() {
    const auto width = runtimeLast(inViewportWidth, 139);
    const auto height = runtimeLast(inViewportHeight, 139);
    return static_cast<std::size_t>(viewportProduct(width, height, 4));
}

void inDumpViewport(std::vector<uint8_t>& dumpTo) {
    const int width = inViewportWidth.empty() ? 0 : runtimeLast(inViewportWidth, 144);
    const int height = inViewportHeight.empty() ? 0 : runtimeLast(inViewportHeight, 145);
    const auto required = viewportProduct(width, height, 4);
    if (dumpTo.size() < static_cast<std::size_t>(required)) {
        throw d_runtime::ThrowableException(
            d_runtime::RetainedString::fromStaticLiteral("Invalid data destination length for inDumpViewport"),
            d_runtime::RetainedString::fromStaticLiteral("nijilive/core/runtime_state.d"), 147);
    }

    requireRenderBackend()->dumpViewport(dumpTo, width, height);

    if (width == 0 || height == 0) return;
    const auto lineSize = static_cast<std::size_t>(viewportProduct(width, 1, 4));
    std::vector<uint8_t> tmpLine(lineSize);
    std::size_t ri = 0;
    for (int upper = height; upper > height / 2;) {
        const int i = --upper;
        const std::size_t oldLineStart = lineSize * ri;
        const std::size_t newLineStart = lineSize * static_cast<std::size_t>(i);
        std::memcpy(tmpLine.data(), dumpTo.data() + oldLineStart, lineSize);
        std::memcpy(dumpTo.data() + oldLineStart, dumpTo.data() + newLineStart, lineSize);
        std::memcpy(dumpTo.data() + newLineStart, tmpLine.data(), lineSize);
        ++ri;
    }
}

void inDumpViewport(common::SharedArray<std::uint8_t>& dumpTo) {
    const int width = inViewportWidth.empty() ? 0 : runtimeLast(inViewportWidth, 144);
    const int height = inViewportHeight.empty() ? 0 : runtimeLast(inViewportHeight, 145);
    const auto required = viewportProduct(width, height, 4);
    if (dumpTo.size() < static_cast<std::size_t>(required)) {
        throw d_runtime::ThrowableException(
            d_runtime::RetainedString::fromStaticLiteral("Invalid data destination length for inDumpViewport"),
            d_runtime::RetainedString::fromStaticLiteral("nijilive/core/runtime_state.d"), 147);
    }
    requireRenderBackend()->dumpViewport(dumpTo, width, height);
    if (width == 0 || height == 0) return;
    const auto lineSize = static_cast<std::size_t>(viewportProduct(width, 1, 4));
    common::SharedArray<std::uint8_t> tmpLine(lineSize, 0);
    std::size_t ri = 0;
    for (int upper = height; upper > height / 2;) {
        const int i = --upper;
        const auto oldLineStart = lineSize * ri;
        const auto newLineStart = lineSize * static_cast<std::size_t>(i);
        // The backend receives the actual ref descriptor and may replace it.
        // Each pointer is read from the current descriptor after that callback.
        std::memcpy(tmpLine.data(), dumpTo.data() + oldLineStart, lineSize);
        std::memcpy(dumpTo.data() + oldLineStart, dumpTo.data() + newLineStart, lineSize);
        std::memcpy(dumpTo.data() + newLineStart, tmpLine.data(), lineSize);
        ++ri;
    }
}

void inPushCamera() {
    inPushCamera(std::make_shared<math::Camera>());
}

void inPushCamera(const math::Camera& camera) {
    inPushCamera(std::make_shared<math::Camera>(camera));
}

void inPushCamera(std::shared_ptr<math::Camera> camera) {
    inCamera.push_back(std::move(camera));
}

void inPopCamera() {
    if (inCamera.size() > 1) inCamera.resize(inCamera.size() - 1);
}

std::shared_ptr<math::Camera> inGetCameraHandle() {
    if (inCamera.empty()) inPushCamera();
    return runtimeLast(inCamera, 65);
}

math::Camera& inGetCamera() {
    return *inGetCameraHandle();
}

void inSetCamera(const math::Camera& camera) {
    inSetCamera(std::make_shared<math::Camera>(camera));
}

void inSetCamera(std::shared_ptr<math::Camera> camera) {
    if (inCamera.empty()) inPushCamera(std::move(camera));
    else runtimeLast(inCamera, 73) = std::move(camera);
}

void inEnsureCameraStackForTests() {
    if (inCamera.empty()) inPushCamera();
}

void inSetClearColor(float r, float g, float b, float a) {
    inClearColor = nodes::Vec4{r, g, b, a};
}

void inGetClearColor(float& r, float& g, float& b, float& a) {
    r = std::numeric_limits<float>::quiet_NaN();
    g = std::numeric_limits<float>::quiet_NaN();
    b = std::numeric_limits<float>::quiet_NaN();
    a = std::numeric_limits<float>::quiet_NaN();
    r = inClearColor.x;
    g = inClearColor.y;
    b = inClearColor.z;
    a = inClearColor.w;
}

RenderResourceHandle inGetRenderImage() {
#ifdef NICXLIVE_SOURCE_NO_RENDER
    return 0;
#else
    auto backend = tryRenderBackend();
    return backend ? backend->renderImageHandle() : 0;
#endif
}

RenderResourceHandle inGetFramebuffer() {
#ifdef NICXLIVE_SOURCE_NO_RENDER
    return 0;
#else
    auto backend = tryRenderBackend();
    return backend ? backend->framebufferHandle() : 0;
#endif
}

RenderResourceHandle inGetCompositeImage() { return 0; }
RenderResourceHandle inGetCompositeFramebuffer() { return 0; }

RenderResourceHandle inGetMainAlbedo() {
#ifdef NICXLIVE_SOURCE_NO_RENDER
    return 0;
#else
    auto backend = tryRenderBackend();
    return backend ? backend->mainAlbedoHandle() : 0;
#endif
}

RenderResourceHandle inGetMainEmissive() {
#ifdef NICXLIVE_SOURCE_NO_RENDER
    return 0;
#else
    auto backend = tryRenderBackend();
    return backend ? backend->mainEmissiveHandle() : 0;
#endif
}

RenderResourceHandle inGetMainBump() {
#ifdef NICXLIVE_SOURCE_NO_RENDER
    return 0;
#else
    auto backend = tryRenderBackend();
    return backend ? backend->mainBumpHandle() : 0;
#endif
}

RenderResourceHandle inGetCompositeEmissive() { return 0; }
RenderResourceHandle inGetCompositeBump() { return 0; }

RenderResourceHandle inGetBlendFramebuffer() {
#ifdef NICXLIVE_SOURCE_NO_RENDER
    return 0;
#else
    auto backend = tryRenderBackend();
    return backend ? backend->blendFramebufferHandle() : 0;
#endif
}

RenderResourceHandle inGetBlendAlbedo() {
#ifdef NICXLIVE_SOURCE_NO_RENDER
    return 0;
#else
    auto backend = tryRenderBackend();
    return backend ? backend->blendAlbedoHandle() : 0;
#endif
}

RenderResourceHandle inGetBlendEmissive() {
#ifdef NICXLIVE_SOURCE_NO_RENDER
    return 0;
#else
    auto backend = tryRenderBackend();
    return backend ? backend->blendEmissiveHandle() : 0;
#endif
}

RenderResourceHandle inGetBlendBump() {
#ifdef NICXLIVE_SOURCE_NO_RENDER
    return 0;
#else
    auto backend = tryRenderBackend();
    return backend ? backend->blendBumpHandle() : 0;
#endif
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
    ngRegisterModelResourceClassNames();
    inPushViewport(0, 0);
    nodes::inInitBlending();
    nodes::Node::inInitNodesSource();
    nodes::inInitDrawable();
    nodes::inInitPart();
    nodes::inInitMask();
    nodes::inInitComposite();
    nodes::inInitMeshGroup();
    nodes::inInitDComposite();
    nodes::inInitGridDeformer();
    nodes::inInitPathDeformer();
    param::inParameterSetFactory([](const serde::Fghj& data) {
        auto parameter = std::make_shared<param::Parameter>();
        if (auto error = parameter->deserializeFromFghj(data)) throw std::runtime_error(*error);
        return parameter;
    });
    param::inParameterSetTypedFactory([](serde::TypedJsonView data) {
        auto parameter = std::make_shared<param::Parameter>();
        (void)parameter->deserializeFromFghj(data);
        return parameter;
    });
    inSetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
}

void initRenderer() {
    initRendererCommon();
    requireRenderBackend()->initializeRenderer();
}

void inSetDifferenceAggregationEnabled(bool enabled) {
    rpSetDifferenceEvaluationEnabled(enabled);
}

bool inIsDifferenceAggregationEnabled() {
    return rpDifferenceEvaluationEnabled();
}

void inSetDifferenceAggregationRegion(int x, int y, int width, int height) {
    rpSetDifferenceEvaluationRegion(DifferenceEvaluationRegion{x, y, width, height});
}

DifferenceEvaluationRegion inGetDifferenceAggregationRegion() {
    return rpGetDifferenceEvaluationRegion();
}

bool inEvaluateDifferenceAggregation(RenderResourceHandle texture, int viewportWidth, int viewportHeight) {
    return rpEvaluateDifference(texture, viewportWidth, viewportHeight);
}

bool inFetchDifferenceAggregationResult(DifferenceEvaluationResult& result) {
    // Source out parameters are initialized before dispatch, including a
    // selected backend that returns without assigning any fields.
    result = DifferenceEvaluationResult{};
    return rpFetchDifferenceResult(result);
}

} // namespace nicxlive::core
