// nijilive core.dbg and nijigenerate core.dbg native port. BSD-2-Clause.
#include "dbg.hpp"
#include "runtime_state.hpp"
#include "render/backend_queue.hpp"
#include "compat/static_error.hpp"
#include "compat/native_access_fault.hpp"
#include <new>
#include <stdexcept>
#include <vector>

namespace nicxlive::core::dbg {
thread_local bool inDbgDrawMeshOutlines = false;
thread_local bool inDbgDrawMeshVertexPoints = false;
thread_local bool inDbgDrawMeshOrientation = false;
namespace {
thread_local bool debugInitialized = false;
thread_local bool hasDebugBuffer = false;
thread_local std::shared_ptr<DebugProvider> selectedProvider;
thread_local std::weak_ptr<nicxlive::core::RenderBackend> selectedRuntimeBackend;

// The authored queue backend intentionally has no debug rendering. Retain its
// no-op contract explicitly, rather than treating unknown backends as implemented.
class QueueDebugProvider final : public DebugProvider {
public:
    void initialize() override {}
    void pointSize(float) override {}
    void lineWidth(float) override {}
    void upload(const Vec3Array&, std::span<const std::uint16_t>) override {}
    void external(RenderResourceHandle, RenderResourceHandle, int) override {}
    void points(Vec4, const Mat4&) override {}
    void lines(Vec4, const Mat4&) override {}
};
std::shared_ptr<DebugProvider> nativeProviderOrNull() {
    const auto runtime = nicxlive::core::tryRenderBackend();
    if (!runtime) return {};
    if (std::dynamic_pointer_cast<nicxlive::core::render::QueueRenderBackend>(runtime)) {
        static thread_local auto queue = std::make_shared<QueueDebugProvider>();
        return queue;
    }
    if (selectedProvider && selectedRuntimeBackend.lock() == runtime) return selectedProvider;
    throw std::logic_error("The active render backend has no selected native debug provider");
}
std::shared_ptr<DebugProvider> nativeProviderForDebug() {
    auto backend = nativeProviderOrNull();
    if (!backend) return {};
    if (!debugInitialized) { backend->initialize(); debugInitialized = true; }
    return backend;
}
std::shared_ptr<RenderBackend> backendOrNull() { return nicxlive::core::tryRenderBackend(); }
std::shared_ptr<RenderBackend> backendForDebug() {
    auto backend = backendOrNull();
    if (!backend) return {};
    if (!debugInitialized) { backend->initDebugRenderer(); debugInitialized = true; }
    return backend;
}
common::SharedArray<std::uint16_t> allocateDebugIndices(std::size_t count) {
    try { return common::SharedArray<std::uint16_t>(count, 0); }
    catch (const std::bad_alloc&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(
            d_runtime::RetainedString::fromStaticLiteral("rt/lifetime.d"), 479);
    } catch (const std::length_error&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(
            d_runtime::RetainedString::fromStaticLiteral("rt/lifetime.d"), 479);
    }
}
}
void ngInstallDebugProvider(std::shared_ptr<DebugProvider> provider,
        std::shared_ptr<nicxlive::core::RenderBackend> runtimeBackend) {
    if (provider && !runtimeBackend)
        throw std::invalid_argument("A debug provider requires an explicit runtime backend identity");
    if (provider && std::dynamic_pointer_cast<nicxlive::core::render::QueueRenderBackend>(runtimeBackend))
        throw std::invalid_argument("The queue backend cannot be replaced by a GL debug provider");
    selectedProvider = std::move(provider);
    selectedRuntimeBackend = runtimeBackend;
    debugInitialized = false;
    hasDebugBuffer = false;
}
void ngClearDebugProvider() { ngInstallDebugProvider({}, {}); }
void inInitDebug() { (void)backendForDebug(); }
void inDbgPointsSize(float size) { if (auto backend = backendForDebug()) backend->setDebugPointSizeSource(size); }
void inDbgLineWidth(float size) { if (auto backend = backendForDebug()) backend->setDebugLineWidthSource(size); }
void inDbgSetBuffer(const Vec3Array& points) {
    const auto actualPoints = points;
    const auto count = actualPoints.size();
    auto indices = allocateDebugIndices(count == 0 ? 0 : count + 1);
    for (std::size_t i = 0; i < count; ++i) {
        if (i >= indices.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
            i, indices.size(), d_runtime::RetainedString::fromStaticLiteral("nijilive/core/dbg.d"), 51);
        indices[i] = static_cast<std::uint16_t>(i);
    }
    if (!indices.empty()) indices[indices.size() - 1] = 0;
    inDbgSetBuffer(actualPoints, indices);
}
void inDbgSetBuffer(RenderResourceHandle vbo, RenderResourceHandle ibo, int count) {
    auto backend = backendForDebug();
    if (!backend) return;
    backend->setDebugExternalBuffer(vbo, ibo, count);
    hasDebugBuffer = count > 0;
}
void inDbgSetBuffer(const Vec3Array& points, std::span<const std::uint16_t> indices) {
    const auto actualPoints = points;
    if (actualPoints.empty() || indices.empty()) { hasDebugBuffer = false; return; }
    auto backend = nativeProviderForDebug();
    if (!backend) return;
    backend->upload(actualPoints, indices);
    hasDebugBuffer = true;
}
void inDbgSetBuffer(Vec3Array points, common::SharedArray<std::uint16_t> indices) {
    if (points.empty() || indices.empty()) { hasDebugBuffer = false; return; }
    auto backend = backendForDebug();
    if (!backend) return;
    // Both Source value descriptors remain held through the actual virtual call
    // even when initialization or upload replaces caller array fields.
    backend->uploadDebugBuffer(points, indices);
    hasDebugBuffer = true;
}
void inDbgSetBuffer(std::span<const Vec3> points) {
    Vec3Array converted;
    converted.resize(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) converted[i].set(points[i]);
    inDbgSetBuffer(converted);
}
void inDbgDrawPoints(Vec4 color, const Mat4& transform) {
    const auto actualTransform = transform;
    if (!hasDebugBuffer) return;
    if (auto backend = backendForDebug()) {
        const auto camera = nicxlive::core::inGetCameraHandle();
        backend->drawDebugPoints(color, d_runtime::ngRequireObject(camera)->matrix() * actualTransform);
    }
}
void inDbgDrawLines(Vec4 color, const Mat4& transform) {
    const auto actualTransform = transform;
    if (!hasDebugBuffer) return;
    if (auto backend = backendForDebug()) {
        const auto camera = nicxlive::core::inGetCameraHandle();
        backend->drawDebugLines(color, d_runtime::ngRequireObject(camera)->matrix() * actualTransform);
    }
}
}
