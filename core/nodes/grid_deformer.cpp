#include "grid_deformer.hpp"
#include "grid_axis_sort.hpp"
#include "../compat/array_range_error.hpp"
#include "../compat/static_error.hpp"

#include "../common/utils.hpp"
#include "../common/object_key_policy.hpp"
#include "../param/parameter.hpp"
#include "../puppet.hpp"
#include "../render.hpp"
#include "../render/profiler.hpp"
#include "../serde.hpp"
#include "path_deformer.hpp"
#include "composite.hpp"

#include <algorithm>
#include <bit>
#include <span>
#include "../compat/assert_error.hpp"
#include <cctype>
#include <cmath>
#include <functional>
#include <limits>
#include <unordered_map>

namespace nicxlive::core::nodes {

using nicxlive::core::common::transformAssign;
using nicxlive::core::common::transformAdd;
using nicxlive::core::common::operator-;

namespace {
constexpr float kAxisTolerance = 1e-4f;
constexpr float kBoundaryTolerance = 1e-4f;
constexpr int kGridFilterStage = 1;

// Same float criterion as Source isClose(a,b,AxisTolerance,AxisTolerance).
bool isCloseAxisValue(float a, float b) {
    if (a == b) return true;
    if (std::isinf(a) || std::isinf(b)) return false;
    const float diff = std::fabs(a - b);
    return diff <= kAxisTolerance * std::fabs(a)
        || diff <= kAxisTolerance * std::fabs(b)
        || diff <= kAxisTolerance;
}

bool isFiniteMatrix(const Mat4& m) {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            if (!std::isfinite(m.a.a[r][c])) return false;
        }
    }
    return true;
}

} // namespace

void GridDeformer::setGridAxes(Axis xs, Axis ys) {
    axisX = normalizeAxis(xs);
    axisY = normalizeAxis(ys);
    rebuildBuffers();
}

void GridDeformer::rebuildVertices() { rebuildBuffers(); }

void GridDeformer::rebuildBuffers() {
    if (axisX.size() < 2) axisX = Axis{kDefaultAxis[0], kDefaultAxis[1]};
    if (axisY.size() < 2) axisY = Axis{kDefaultAxis[0], kDefaultAxis[1]};
    vertices.resize(cols() * rows());
    const auto vertexCount = vertices.size();
    getDeformation().resize(vertexCount);
    auto offsets = getDeformation();
    const auto offsetCount = offsets.size();
    for (std::size_t i = 0; i < offsetCount; ++i) offsets[i] = Vec2{0, 0};
    const auto rowCount = rows();
    for (std::size_t y = 0; y < rowCount; ++y) {
        const auto columnCount = cols();
        for (std::size_t x = 0; x < columnCount; ++x) {
            const auto index = gridIndex(x, y);
            const Vec2 point{sourceGridAt(axisX, x, 424), sourceGridAt(axisY, y, 424)};
            vertices[index] = point;
        }
    }
}

namespace {
Node::FilterArray gridSourceRemoveFilter(Node::FilterArray filters, const Node::FilterHook& value) {
    for (std::size_t i = 0; i < filters.size(); ++i) {
        const auto current = filters[i];
        if (current.stage == value.stage && current.func == value.func) return filters.removed(i);
    }
    return filters;
}

Node::FilterArray gridSourceUpsertFilter(Node::FilterArray filters, const Node::FilterHook& value, bool prepend) {
    for (const auto current : filters)
        if (current.stage == value.stage && current.func == value.func) return filters;
    if (prepend) filters.insertValue(0, value);
    else filters.push_back(value);
    return filters;
}
}

GridDeformer::GridDeformer() {
    requirePreProcessTask();
    axisX = Axis{kDefaultAxis[0], kDefaultAxis[1]};
    axisY = Axis{kDefaultAxis[0], kDefaultAxis[1]};
    setGridAxes(axisX, axisY);
}

std::shared_ptr<GridDeformer> GridDeformer::create(std::shared_ptr<Node> parent) {
    ngRegisterModelResourceClassNames();
    auto result = std::shared_ptr<GridDeformer>(new GridDeformer(DeferredConstruction{}));
    result->constructGridOwned(parent);
    return result;
}

std::shared_ptr<GridDeformer> GridDeformer::allocateSourceGridReceiver() {
    ngRegisterModelResourceClassNames();
    return std::allocate_shared<GridDeformer>(
        d_runtime::SourceAllocator<GridDeformer>{}, DeferredConstruction{});
}

void GridDeformer::constructGridOwned(std::shared_ptr<Node> parent) {
    [[maybe_unused]] const auto lifetime = weak_from_this().lock();
    constructDeformableOwned(parent);
    requirePreProcessTask();
    axisX = Axis{kDefaultAxis[0], kDefaultAxis[1]};
    axisY = Axis{kDefaultAxis[0], kDefaultAxis[1]};
    setGridAxes(axisX, axisY);
}

void GridDeformer::setGridAxes(const std::vector<float>& xs, const std::vector<float>& ys) {
    Axis xValues(xs.size(), std::numeric_limits<float>::quiet_NaN());
    std::copy(xs.begin(), xs.end(), xValues.begin());
    Axis yValues(ys.size(), std::numeric_limits<float>::quiet_NaN());
    std::copy(ys.begin(), ys.end(), yValues.begin());
    setGridAxes(xValues, yValues);
}

void GridDeformer::switchDynamic(bool value) {
    [[maybe_unused]] const auto lifetime = weak_from_this().lock();
    dynamic = value;
    const auto children = childrenRef();
    for (auto child : children) setupChildNoRecurse(child);
}

void GridDeformer::rebuffer(const Vec2Array& gridPoints) {
    [[maybe_unused]] const auto lifetime = weak_from_this().lock();
    auto points = gridPoints;
    if (points.empty() || !adoptFromVertices(points, false)) {
        adoptGridFromAxes(Axis{kDefaultAxis[0], kDefaultAxis[1]}, Axis{kDefaultAxis[0], kDefaultAxis[1]});
    }
    clearCache();
}

GridDeformer::GridCellCache GridDeformer::computeCache(float localX, float localY) const {
    GridCellCache cache;
    if (!hasValidGrid()) return cache;
    if (!std::isfinite(localX) || !std::isfinite(localY)) return cache;
    auto clampVal = [](float v, float min, float max) {
        if (v < min) return min;
        if (v > max) return max;
        return v;
    };
    float clampedX = clampVal(localX, axisX[0], axisX[axisX.size() - 1]);
    float clampedY = clampVal(localY, axisY[0], axisY[axisY.size() - 1]);

    auto rx = locateInterval(axisX, clampedX);
    auto ry = locateInterval(axisY, clampedY);
    if (!rx.valid || !ry.valid) return cache;

    float u = rx.weight;
    float v = ry.weight;
    if (u <= kBoundaryTolerance) u = 0;
    else if (u >= 1 - kBoundaryTolerance) u = 1;
    if (v <= kBoundaryTolerance) v = 0;
    else if (v >= 1 - kBoundaryTolerance) v = 1;

    cache.cellX = rx.index;
    cache.cellY = ry.index;
    cache.u = u;
    cache.v = v;
    cache.valid = true;
    return cache;
}

void GridDeformer::sampleGridPoints(Vec2Array& dst, const std::vector<GridCellCache>& caches, bool includeDeformation) const {
    dst.resize(caches.size());
    std::size_t cols = axisX.size();
    constexpr std::size_t kSimdWidth = 4;

    std::size_t len = caches.size();
    std::size_t i = 0;
    for (; i + kSimdWidth <= len; i += kSimdWidth) {
        float w00[kSimdWidth]{}, w10[kSimdWidth]{}, w01[kSimdWidth]{}, w11[kSimdWidth]{};
        float p00x[kSimdWidth]{}, p10x[kSimdWidth]{}, p01x[kSimdWidth]{}, p11x[kSimdWidth]{};
        float p00y[kSimdWidth]{}, p10y[kSimdWidth]{}, p01y[kSimdWidth]{}, p11y[kSimdWidth]{};
        bool valid[kSimdWidth]{};

        for (std::size_t lane = 0; lane < kSimdWidth; ++lane) {
            const auto& c = caches[i + lane];
            if (!c.valid) continue;
            valid[lane] = true;
            float u = c.u;
            float v = c.v;
            float wu0 = 1.0f - u;
            float wv0 = 1.0f - v;
            w00[lane] = wu0 * wv0;
            w10[lane] = u * wv0;
            w01[lane] = wu0 * v;
            w11[lane] = u * v;

            auto x = c.cellX;
            auto y = c.cellY;
            auto idx00 = y * cols + x;
            auto idx10 = y * cols + (x + 1);
            auto idx01 = (y + 1) * cols + x;
            auto idx11 = (y + 1) * cols + (x + 1);

            float p00xVal = vertices.xAt(idx00), p00yVal = vertices.yAt(idx00);
            float p10xVal = vertices.xAt(idx10), p10yVal = vertices.yAt(idx10);
            float p01xVal = vertices.xAt(idx01), p01yVal = vertices.yAt(idx01);
            float p11xVal = vertices.xAt(idx11), p11yVal = vertices.yAt(idx11);
            if (includeDeformation) {
                p00xVal += deformation.xAt(idx00); p00yVal += deformation.yAt(idx00);
                p10xVal += deformation.xAt(idx10); p10yVal += deformation.yAt(idx10);
                p01xVal += deformation.xAt(idx01); p01yVal += deformation.yAt(idx01);
                p11xVal += deformation.xAt(idx11); p11yVal += deformation.yAt(idx11);
            }
            p00x[lane] = p00xVal; p00y[lane] = p00yVal;
            p10x[lane] = p10xVal; p10y[lane] = p10yVal;
            p01x[lane] = p01xVal; p01y[lane] = p01yVal;
            p11x[lane] = p11xVal; p11y[lane] = p11yVal;
        }

        for (std::size_t lane = 0; lane < kSimdWidth; ++lane) {
            if (!valid[lane]) {
                dst.xAt(i + lane) = 0;
                dst.yAt(i + lane) = 0;
                continue;
            }
            dst.xAt(i + lane) = p00x[lane] * w00[lane] + p10x[lane] * w10[lane] + p01x[lane] * w01[lane] + p11x[lane] * w11[lane];
            dst.yAt(i + lane) = p00y[lane] * w00[lane] + p10y[lane] * w10[lane] + p01y[lane] * w01[lane] + p11y[lane] * w11[lane];
        }
    }

    for (; i < len; ++i) {
        const auto& c = caches[i];
        if (!c.valid) { dst.xAt(i) = 0; dst.yAt(i) = 0; continue; }
        auto idx00 = c.cellY * cols + c.cellX;
        auto idx10 = c.cellY * cols + (c.cellX + 1);
        auto idx01 = (c.cellY + 1) * cols + c.cellX;
        auto idx11 = (c.cellY + 1) * cols + (c.cellX + 1);
        float p00xVal = vertices.xAt(idx00), p00yVal = vertices.yAt(idx00);
        float p10xVal = vertices.xAt(idx10), p10yVal = vertices.yAt(idx10);
        float p01xVal = vertices.xAt(idx01), p01yVal = vertices.yAt(idx01);
        float p11xVal = vertices.xAt(idx11), p11yVal = vertices.yAt(idx11);
        if (includeDeformation) {
            p00xVal += deformation.xAt(idx00); p00yVal += deformation.yAt(idx00);
            p10xVal += deformation.xAt(idx10); p10yVal += deformation.yAt(idx10);
            p01xVal += deformation.xAt(idx01); p01yVal += deformation.yAt(idx01);
            p11xVal += deformation.xAt(idx11); p11yVal += deformation.yAt(idx11);
        }
        float u = c.u, v = c.v;
        float wu0 = 1.0f - u;
        float wv0 = 1.0f - v;
        float w00 = wu0 * wv0;
        float w10 = u * wv0;
        float w01 = wu0 * v;
        float w11 = u * v;
        dst.xAt(i) = p00xVal * w00 + p10xVal * w10 + p01xVal * w01 + p11xVal * w11;
        dst.yAt(i) = p00yVal * w00 + p10yVal * w10 + p01yVal * w01 + p11yVal * w11;
    }
}

void GridDeformer::sampleGridPointsSource(Vec2Array& dst,
    const std::vector<GridCellCache>& caches, bool includeDeformation) const {
    dst.resize(caches.size());
    if (caches.empty()) return;
    // Preserve Source lane capture order before virtual property callbacks.
    const auto base = vertices;
    const auto deformationX = getDeformation();
    const auto deformationY = getDeformation();
    const auto cols = axisX.size();
    const auto read = [](const Vec2Array& values, std::size_t index, bool y, std::size_t line) {
        if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
            index, values.size(), d_runtime::RetainedString::fromStaticLiteral(
                "source/nijilive/core/nodes/deformer/grid.d"), line);
        return y ? values.yAt(index) : values.xAt(index);
    };
    const auto sample = [&](const GridCellCache& cache, float* weights, float* xs, float* ys,
                            std::size_t baseLine, std::size_t deformLine) {
        const float u = cache.u, v = cache.v;
        const float wu0 = 1 - u, wv0 = 1 - v;
        weights[0] = wu0 * wv0; weights[1] = u * wv0;
        weights[2] = wu0 * v; weights[3] = u * v;
        const std::size_t indices[]{cache.cellY * cols + cache.cellX,
            cache.cellY * cols + cache.cellX + 1,
            (cache.cellY + 1) * cols + cache.cellX,
            (cache.cellY + 1) * cols + cache.cellX + 1};
        for (std::size_t corner = 0; corner < 4; ++corner) {
            xs[corner] = read(base, indices[corner], false, baseLine + corner * 2);
            ys[corner] = read(base, indices[corner], true, baseLine + corner * 2 + 1);
        }
        if (includeDeformation) for (std::size_t corner = 0; corner < 4; ++corner) {
            xs[corner] += read(deformationX, indices[corner], false, deformLine + corner * 2);
            ys[corner] += read(deformationY, indices[corner], true, deformLine + corner * 2 + 1);
        }
    };
    const auto blend = [](const float* weights, const float* values) {
        return weights[0] * values[0] + weights[1] * values[1]
            + weights[2] * values[2] + weights[3] * values[3];
    };
    std::size_t i = 0;
    for (; i + 4 <= caches.size(); i += 4) {
        float weights[4][4], xs[4][4], ys[4][4];
        for (std::size_t lane = 0; lane < 4; ++lane) {
            // Source SimdRepr's float vector defaults to float.init, including
            // lanes skipped by the invalid-cache branch before the final blend.
            std::fill_n(weights[lane], 4, std::numeric_limits<float>::quiet_NaN());
            std::fill_n(xs[lane], 4, std::numeric_limits<float>::quiet_NaN());
            std::fill_n(ys[lane], 4, std::numeric_limits<float>::quiet_NaN());
            if (caches[i + lane].valid)
                sample(caches[i + lane], weights[lane], xs[lane], ys[lane], 660, 670);
        }
        for (std::size_t lane = 0; lane < 4; ++lane) dst.xAt(i + lane) = blend(weights[lane], xs[lane]);
        for (std::size_t lane = 0; lane < 4; ++lane) dst.yAt(i + lane) = blend(weights[lane], ys[lane]);
    }
    for (; i < caches.size(); ++i) {
        if (!caches[i].valid) { dst.xAt(i) = 0; dst.yAt(i) = 0; continue; }
        float weights[4], xs[4], ys[4];
        sample(caches[i], weights, xs, ys, 717, 727);
        dst.xAt(i) = blend(weights, xs);
        dst.yAt(i) = blend(weights, ys);
    }
}

SourceDeformResult GridDeformer::deformChildrenSource(std::shared_ptr<Node> target,
    Vec2Array origVertices, Vec2Array origDeformation, Mat4* origTransform) {
    auto profile = core::render::profileScope("GridDeformer.deformChildrenSource");
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (!hasValidGrid()) return {Vec2Array{}, nullptr, false};
    if (auto path = std::dynamic_pointer_cast<PathDeformer>(target)) {
        if (!path->physicsEnabled()) return {Vec2Array{}, nullptr, false};
    }
    [[maybe_unused]] const auto targetName = target ? target->getName()
        : common::NullableString::fromStaticLiteral("(null)");
    if (!matrixIsFinite(inverseMatrix)) return {Vec2Array{}, nullptr, false};
    if (!origTransform) return {Vec2Array{}, nullptr, false};
    if (!matrixIsFinite(*origTransform)) return {Vec2Array{}, nullptr, false};
    const Mat4 centerMatrix = Mat4::multiply(inverseMatrix, *origTransform);
    std::vector<GridCellCache> caches(origVertices.size());
    Vec2Array samplePoints;
    bool anyChanged = false;
    if (!matrixIsFinite(centerMatrix)) return {Vec2Array{}, nullptr, false};
    transformAssign(samplePoints, origVertices, centerMatrix);
    if (dynamic && !origDeformation.empty() && !samplePoints.empty()) {
        const auto overlap = std::min(origDeformation.size(), samplePoints.size());
        transformAdd(samplePoints, origDeformation, centerMatrix, overlap);
    }
    bool invalidSamples = false;
    for (std::size_t i = 0; i < samplePoints.size(); ++i) {
        const auto x = samplePoints.xAt(i), y = samplePoints.yAt(i);
        if (!std::isfinite(x) || !std::isfinite(y)) { invalidSamples = true; break; }
        auto cache = computeCache(x, y);
        cache.cellX = static_cast<std::uint16_t>(cache.cellX);
        cache.cellY = static_cast<std::uint16_t>(cache.cellY);
        caches[i] = cache;
    }
    if (invalidSamples) return {Vec2Array{}, nullptr, false};
    Vec2Array originalSample, targetSample;
    sampleGridPointsSource(originalSample, caches, false);
    sampleGridPointsSource(targetSample, caches, true);
    auto offsetLocal = targetSample.dup();
    offsetLocal -= originalSample;
    for (std::size_t i = 0; i < offsetLocal.size(); ++i) {
        if (!caches[i].valid || !std::isfinite(offsetLocal.xAt(i)) || !std::isfinite(offsetLocal.yAt(i))) {
            offsetLocal.xAt(i) = 0; offsetLocal.yAt(i) = 0; continue;
        }
        if (offsetLocal.xAt(i) != 0 || offsetLocal.yAt(i) != 0) anyChanged = true;
    }
    if (!anyChanged) return {origDeformation, nullptr, false};
    auto inverse = centerMatrix.inverse();
    inverse[0][3] = 0; inverse[1][3] = 0; inverse[2][3] = 0;
    transformAdd(origDeformation, offsetLocal, inverse, offsetLocal.size());
    return {origDeformation, nullptr, anyChanged};
}

DeformResult GridDeformer::deformChildren(const std::shared_ptr<Node>& target,
                                          const Vec2Array& origVertices,
                                          Vec2Array& origDeformation,
                                          const Mat4* origTransform) {
    auto scope = core::render::profileScope("GridDeformer.deformChildren");
    DeformResult res;

    if (!hasValidGrid() || !origTransform) return res;
    if (auto path = std::dynamic_pointer_cast<PathDeformer>(target)) {
        if (!path->physicsEnabled()) return res;
    }
    if (!matrixIsFinite(inverseMatrix) || !matrixIsFinite(*origTransform)) return res;

    Mat4 centerMatrix = Mat4::multiply(inverseMatrix, *origTransform);
    if (!matrixIsFinite(centerMatrix)) return res;

    Vec2Array samplePoints;
    transformAssign(samplePoints, origVertices, centerMatrix);
    if (dynamic && !origDeformation.empty() && samplePoints.size()) {
        std::size_t overlap = std::min<std::size_t>(samplePoints.size(), origDeformation.size());
        transformAdd(samplePoints, origDeformation, centerMatrix, overlap);
    }

    std::vector<GridCellCache> caches(samplePoints.size());
    bool anyChanged = false;
    bool invalidSamples = false;
    for (std::size_t i = 0; i < samplePoints.size(); ++i) {
        const auto sx = samplePoints.xAt(i);
        const auto sy = samplePoints.yAt(i);
        if (!std::isfinite(sx) || !std::isfinite(sy)) {
            invalidSamples = true;
            break;
        }
        caches[i] = computeCache(sx, sy);
    }
    if (invalidSamples) return res;

    Vec2Array originalSample;
    Vec2Array targetSample;
    sampleGridPoints(originalSample, caches, false);
    sampleGridPoints(targetSample, caches, true);
    Vec2Array offsetLocal = targetSample - originalSample;
    for (std::size_t i = 0; i < offsetLocal.size(); ++i) {
        if (!caches[i].valid || !std::isfinite(offsetLocal.xAt(i)) || !std::isfinite(offsetLocal.yAt(i))) {
            offsetLocal.xAt(i) = 0;
            offsetLocal.yAt(i) = 0;
        } else if (offsetLocal.xAt(i) != 0 || offsetLocal.yAt(i) != 0) {
            anyChanged = true;
        }
    }
    if (!anyChanged) return res;

    Mat4 inv = centerMatrix.inverse();
    inv[0][3] = inv[1][3] = inv[2][3] = 0.0f;
    if (origDeformation.size() < offsetLocal.size()) {
        origDeformation.resize(offsetLocal.size());
    }
    transformAdd(origDeformation, offsetLocal, inv, offsetLocal.size());
    res.changed = true;
    return res;
}

void GridDeformer::runPreProcessTask(core::RenderContext& ctx) {
    auto scope = core::render::profileScope("GridDeformer.runPreProcessTask");
    Deformable::runPreProcessTask(ctx);
    localTransform.update();
    transform();
    inverseMatrix = globalTransform.toMat4().inverse();
    updateDeform();
}

void GridDeformer::runRenderTask(core::RenderContext&) {
    // no GPU commands
}

bool GridDeformer::setupChild(const std::shared_ptr<Node>& child) {
    [[maybe_unused]] const auto lifetime = weak_from_this().lock();
    const auto target = child;
    Deformable::setupChild(target);
    auto setGroup = [&](auto&& recurse, std::shared_ptr<Node> current) -> void {
        const bool propagate = d_runtime::ngRequireObject(current)->mustPropagate();
        setupChildNoRecurse(current);
        if (propagate) {
            const auto children = current->childrenList();
            for (auto descendant : children) recurse(recurse, descendant);
        }
    };
    if (hasValidGrid()) setGroup(setGroup, target);
    return false;
}

bool GridDeformer::releaseChild(const std::shared_ptr<Node>& child) {
    [[maybe_unused]] const auto lifetime = weak_from_this().lock();
    const auto target = child;
    auto unsetGroup = [&](auto&& recurse, std::shared_ptr<Node> current) -> void {
        releaseChildNoRecurse(current);
        const bool propagate = d_runtime::ngRequireObject(current)->mustPropagate();
        if (propagate) {
            const auto children = current->childrenList();
            for (auto descendant : children) recurse(recurse, descendant);
        }
    };
    unsetGroup(unsetGroup, target);
    Deformable::releaseChild(target);
    return false;
}

void GridDeformer::captureTarget(const std::shared_ptr<Node>& target) {
    [[maybe_unused]] const auto lifetime = weak_from_this().lock();
    const auto value = target;
    childrenRef().push_back(value);
    setupChildNoRecurse(value, true);
}

void GridDeformer::releaseTarget(const std::shared_ptr<Node>& target) {
    [[maybe_unused]] const auto lifetime = weak_from_this().lock();
    const auto value = target;
    releaseChildNoRecurse(value);
    const auto range = childrenRef();
    std::ptrdiff_t found = -1;
    for (std::size_t i = 0; i < range.size(); ++i) {
        if (common::ngClassObjectEquals(range[i], value, ngRegisterModelResourceClassNames)) {
            found = static_cast<std::ptrdiff_t>(i);
            break;
        }
    }
    childrenRef() = found < 0 ? range : range.removed(static_cast<std::size_t>(found));
}

void GridDeformer::clearCache() {
    // no extra caches yet
}

void GridDeformer::dispose() {
    const auto lifetime = weak_from_this().lock();
    const auto range = children;
    for (auto child : range) releaseChild(child);
    childrenRef().resize(0);
}

void GridDeformer::build(bool force) {
    [[maybe_unused]] const auto lifetime = weak_from_this().lock();
    const auto range = childrenList();
    for (auto child : range) setupChild(child);
    setupSelf();
    Deformable::build(force);
}

void GridDeformer::setupChildNoRecurse(const std::shared_ptr<Node>& node, bool prepend) {
    [[maybe_unused]] const auto lifetime = weak_from_this().lock();
    const auto target = node;
    if (auto composite = std::dynamic_pointer_cast<Composite>(target)) {
        if (composite->propagateMeshGroupEnabled()) {
            releaseChildNoRecurse(target);
            return;
        }
    }
    const auto drawable = std::dynamic_pointer_cast<Deformable>(target);
    const auto self = std::static_pointer_cast<GridDeformer>(shared_from_this());
    Node::FilterHook hook;
    hook.stage = 1;
    hook.func = Node::FilterHook::Func::fromMethod<&SourceDeformation::deformChildrenSource>(self);
    if (drawable) {
        if (dynamic) {
            auto post = d_runtime::ngRequireObject(target)->postProcessFilters;
            target->postProcessFilters = gridSourceUpsertFilter(post, hook, prepend);
            auto pre = target->preProcessFilters;
            target->preProcessFilters = gridSourceRemoveFilter(pre, hook);
        } else {
            auto pre = d_runtime::ngRequireObject(target)->preProcessFilters;
            target->preProcessFilters = gridSourceUpsertFilter(pre, hook, prepend);
            auto post = target->postProcessFilters;
            target->postProcessFilters = gridSourceRemoveFilter(post, hook);
        }
    } else if (translateChildren) {
        auto pre = d_runtime::ngRequireObject(target)->preProcessFilters;
        target->preProcessFilters = gridSourceUpsertFilter(pre, hook, prepend);
        auto post = target->postProcessFilters;
        target->postProcessFilters = gridSourceRemoveFilter(post, hook);
    } else releaseChildNoRecurse(target);
}

void GridDeformer::releaseChildNoRecurse(const std::shared_ptr<Node>& node) {
    [[maybe_unused]] const auto lifetime = weak_from_this().lock();
    const auto target = node;
    const auto self = std::static_pointer_cast<GridDeformer>(shared_from_this());
    Node::FilterHook hook;
    hook.stage = 1;
    hook.func = Node::FilterHook::Func::fromMethod<&SourceDeformation::deformChildrenSource>(self);
    auto pre = d_runtime::ngRequireObject(target)->preProcessFilters;
    target->preProcessFilters = gridSourceRemoveFilter(pre, hook);
    auto post = target->postProcessFilters;
    target->postProcessFilters = gridSourceRemoveFilter(post, hook);
}

void GridDeformer::applyDeformToChildren(common::SharedArray<std::shared_ptr<core::param::Parameter>> params,
    bool recursive) {
    const auto self = std::static_pointer_cast<GridDeformer>(shared_from_this());
    auto update = [self](Vec2Array values) {
        if (values.size() != self->getDeformation().size()) return;
        const auto count = values.size();
        for (std::size_t i = 0; i < count; ++i) {
            auto value = values[i];
            self->getDeformation()[i] = value;
        }
    };
    auto transfer = [self] { return self->translateChildren; };
    localTransform.update();
    transform();
    inverseMatrix = globalTransform.toMat4().inverse();
    Node::FilterHook hook;
    hook.stage = 1;
    hook.func = Node::FilterHook::Func::fromMethod<&SourceDeformation::deformChildrenSource>(self);
    _applyDeformToChildren(hook, NodeFilter::SourceUpdate(update),
        NodeFilter::SourceTransferCondition(transfer), params, recursive);
}

void GridDeformer::serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const {
    Deformable::serializeSelfImpl(serializer, recursive, flags);
    serializer.putKey("grid_axis_x");
    auto xs = serializer.listBegin();
    for (float v : axisX) {
        serializer.elemBegin();
        serializer.putValue(v);
    }
    serializer.listEnd(xs);

    serializer.putKey("grid_axis_y");
    auto ys = serializer.listBegin();
    for (float v : axisY) {
        serializer.elemBegin();
        serializer.putValue(v);
    }
    serializer.listEnd(ys);

    serializer.putKey("formation");
    serializer.putValue(static_cast<int>(formation));
    serializer.putKey("dynamic");
    serializer.putValue(dynamic);
}

::nicxlive::core::serde::SerdeException GridDeformer::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    if (auto err = Deformable::deserializeFromFghj(data)) return err;

    if (auto xs = data.get_child_optional("grid_axis_x")) {
        axisX.clear();
        for (const auto& elem : *xs) {
            axisX.push_back(elem.second.get_value<float>());
        }
    }
    if (auto ys = data.get_child_optional("grid_axis_y")) {
        axisY.clear();
        for (const auto& elem : *ys) {
            axisY.push_back(elem.second.get_value<float>());
        }
    }
    if (auto fmText = data.get_optional<std::string>("formation")) {
        std::string normalized;
        normalized.reserve(fmText->size());
        for (char ch : *fmText) {
            if (std::isalnum(static_cast<unsigned char>(ch))) {
                normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            }
        }
        if (normalized == "bilinear") {
            formation = GridFormation::Bilinear;
        }
    } else if (auto fmValue = data.get_optional<int>("formation")) {
        int v = *fmValue;
        if (v >= 0 && v <= static_cast<int>(GridFormation::Bilinear)) {
            formation = static_cast<GridFormation>(v);
        }
    }
    if (auto dyn = data.get_child_optional("dynamic")) {
        dynamic = dyn->get_value<bool>();
    }
    setGridAxes(axisX, axisY);
    clearCache();
    return std::nullopt;
}

std::optional<GridDeformer::GridCellCache> GridDeformer::locate(float x, float y) const {
    auto cache = computeCache(x, y);
    if (!cache.valid) return std::nullopt;
    return cache;
}

GridDeformer::Axis GridDeformer::normalizeAxis(Axis values) {
    auto sorted = values.duplicate();
    if (sorted.empty()) return sorted;
    grid_axis_detail::sort(std::span<float>(sorted.data(), sorted.size()));
    std::size_t write = 1;
    const auto count = sorted.size();
    for (std::size_t i = 1; i < count; ++i) {
        if (!isCloseAxisValue(sourceGridAt(sorted, write - 1, 391), sourceGridAt(sorted, i, 391))) {
            sourceGridAt(sorted, write, 392) = sourceGridAt(sorted, i, 392);
            ++write;
        }
    }
    sorted.resize(write);
    return sorted;
}

int GridDeformer::axisIndexOfValue(Axis axis, float value) const {
    for (std::size_t i = 0; i < axis.size(); ++i) {
        if (isCloseAxisValue(axis[i], value))
            return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(i));
    }
    return -1;
}
void GridDeformer::adoptGridFromAxes(Axis xs, Axis ys) { setGridAxes(xs, ys); }

GridDeformer::IntervalResult GridDeformer::locateInterval(Axis axis, float value) const {
    IntervalResult result;
    if (axis.size() < 2) return result;
    if (value < axis[0] || value > axis[axis.size() - 1]) return result;
    std::size_t index = axis.size() - 2;
    const auto count = axis.size() - 1;
    for (std::size_t i = 0; i < count; ++i) {
        if (value <= axis[i + 1]) { index = i; break; }
    }
    const float span = axis[index + 1] - axis[index];
    const float weight = span > 0 ? (value - axis[index]) / span : 0;
    result.index = index;
    result.weight = weight;
    result.valid = true;
    return result;
}

bool GridDeformer::matrixIsFinite(const Mat4& m) const {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            if (!std::isfinite(m.a.a[r][c])) return false;
        }
    }
    return true;
}

void GridDeformer::copyFrom(const Node& src, bool clone, bool deepCopy) {
    [[maybe_unused]] const auto lifetime = weak_from_this().lock();
    [[maybe_unused]] const auto sourceLifetime = src.weak_from_this().lock();
    auto& source = const_cast<Node&>(src);
    Deformable::copyFrom(src, clone, deepCopy);
    bool initialized = false;
    if (auto* grid = dynamic_cast<GridDeformer*>(&source)) {
        adoptGridFromAxes(grid->axisX, grid->axisY);
        formation = grid->formation;
        dynamic = grid->dynamic;
        const auto offsets = grid->getDeformation().dup();
        getDeformation() = offsets;
        translateChildren = grid->translateChildren;
        initialized = true;
    } else if (auto* drawable = dynamic_cast<Drawable*>(&source)) {
        if (adoptFromVertices(drawable->getVertices(), true)) initialized = true;
    } else if (auto* deformable = dynamic_cast<Deformable*>(&source)) {
        if (adoptFromVertices(deformable->getVertices(), true)) initialized = true;
    }
    if (!initialized)
        adoptGridFromAxes(Axis{kDefaultAxis[0], kDefaultAxis[1]}, Axis{kDefaultAxis[0], kDefaultAxis[1]});
    clearCache();
}

void GridDeformer::serializeSelfImpl(serde::TypedJsonWriter& serializer,
    bool recursive, SerializeNodeFlags flags) const {
    Deformable::serializeSelfImpl(serializer, recursive, flags);
    serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("grid_axis_x"));
    auto xsState = serializer.listBegin();
    const auto xs = axisX;
    for (auto value : xs) { serializer.elemBegin(); serializer.serializeValue(value); }
    serializer.listEnd(xsState);
    serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("grid_axis_y"));
    auto ysState = serializer.listBegin();
    const auto ys = axisY;
    for (auto value : ys) { serializer.elemBegin(); serializer.serializeValue(value); }
    serializer.listEnd(ysState);
    serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("formation"));
    if (formation != GridFormation::Bilinear)
        d_runtime::ngAssertUnreachable("Assertion failure", "mir/serde.d", 522);
    serializer.putValue(d_runtime::RetainedString::fromStaticLiteral("Bilinear"));
    serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("dynamic"));
    serializer.serializeValue(dynamic);
}

} // namespace nicxlive::core::nodes

namespace nicxlive::core::nodes {
serde::ReadError GridDeformer::deserializeFromFghj(serde::TypedJsonView data) {
    if (auto error = Deformable::deserializeFromFghj(data)) return error;
    const auto readAxis = [&](std::string_view key, Axis& axis) -> serde::ReadError {
        if (data.findFirst(key).isMissing()) return {};
        axis.resize(0);
        const auto elements = data.findFirst(key).byElement();
        for (auto element : elements) {
            float value = std::numeric_limits<float>::quiet_NaN();
            if (auto error = serde::deserializeValue(element, value)) return error;
            axis.push_back(value);
        }
        return {};
    };
    if (auto error = readAxis("grid_axis_x", axisX)) return error;
    if (auto error = readAxis("grid_axis_y", axisY)) return error;
    if (!data.findFirst("formation").isMissing()) {
        const auto input = data.findFirst("formation");
        const auto text = input.kind() == serde::JsonKind::String ? input.stringBytes() : std::string_view{};
        if (text != "Bilinear") {
            std::string message = "Unable to deserialize string '";
            message.append(text);
            message += "' to GridFormationAllowed keys:allowedKeys";
            throw d_runtime::ThrowableException(d_runtime::RetainedString(std::move(message)),
                d_runtime::RetainedString::fromStaticLiteral("fghj/serialization.d"), 2167);
        }
        formation = GridFormation::Bilinear;
    }
    if (!data.findFirst("dynamic").isMissing()) {
        if (auto error = serde::deserializeValue(data.findFirst("dynamic"), dynamic)) return error;
    }
    setGridAxes(axisX, axisY);
    clearCache();
    return {};
}
} // namespace nicxlive::core::nodes
