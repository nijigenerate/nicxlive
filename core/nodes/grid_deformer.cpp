#include "grid_deformer.hpp"

#include "../common/utils.hpp"
#include "../param/parameter.hpp"
#include "../render.hpp"
#include "../serde.hpp"
#include "path_deformer.hpp"
#include "composite.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <unordered_map>

namespace nicxlive::core::nodes {

using nicxlive::core::common::transformAssign;
using nicxlive::core::common::transformAdd;
using nicxlive::core::common::operator-;

namespace {
constexpr float kAxisTolerance = 1e-4f;
constexpr float kBoundaryTolerance = 1e-4f;
constexpr int kGridFilterStage = 1;

bool traceDeformerSummaryEnabled() {
    static int enabled = -1;
    if (enabled >= 0) return enabled != 0;
    const char* v = std::getenv("NJCX_TRACE_DEFORMER_SUMMARY");
    if (!v) {
        enabled = 0;
        return false;
    }
    enabled = (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 || std::strcmp(v, "TRUE") == 0) ? 1 : 0;
    return enabled != 0;
}

std::vector<Vec2> toVec2List(const Vec2Array& arr) {
    std::vector<Vec2> out;
    out.reserve(arr.size());
    for (std::size_t i = 0; i < arr.size(); ++i) {
        out.push_back(Vec2{arr.xAt(i), arr.yAt(i)});
    }
    return out;
}

Vec2Array toVec2Array(const std::vector<Vec2>& in) {
    Vec2Array out;
    out.resize(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        out.xAt(i) = in[i].x;
        out.yAt(i) = in[i].y;
    }
    return out;
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

GridDeformer::GridDeformer() {
    axisX.assign(kDefaultAxis.begin(), kDefaultAxis.end());
    axisY.assign(kDefaultAxis.begin(), kDefaultAxis.end());
    setGridAxes(axisX, axisY);
    requirePreProcessTask();
}

void GridDeformer::setGridAxes(const std::vector<float>& xs, const std::vector<float>& ys) {
    axisX = normalizeAxis(xs);
    axisY = normalizeAxis(ys);
    if (axisX.size() < 2) axisX.assign(kDefaultAxis.begin(), kDefaultAxis.end());
    if (axisY.size() < 2) axisY.assign(kDefaultAxis.begin(), kDefaultAxis.end());
    vertices.resize(axisX.size() * axisY.size());
    deformation.resize(vertices.size());
    for (std::size_t y = 0; y < axisY.size(); ++y) {
        for (std::size_t x = 0; x < axisX.size(); ++x) {
            auto idx = y * axisX.size() + x;
            vertices.xAt(idx) = axisX[x];
            vertices.yAt(idx) = axisY[y];
            deformation.set(idx, Vec2{0.0f, 0.0f});
        }
    }
}

void GridDeformer::switchDynamic(bool value) {
    if (dynamic == value) return;
    dynamic = value;
    for (auto& c : children) setupChild(c);
}

void GridDeformer::rebuffer(const Vec2Array& gridPoints) {
    if (gridPoints.size() == 0 || !adoptFromVertices(gridPoints, false)) {
        adoptGridFromAxes(std::vector<float>(kDefaultAxis.begin(), kDefaultAxis.end()),
                          std::vector<float>(kDefaultAxis.begin(), kDefaultAxis.end()));
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
    float clampedX = clampVal(localX, axisX.front(), axisX.back());
    float clampedY = clampVal(localY, axisY.front(), axisY.back());

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
    auto bx = vertices.x;
    auto by = vertices.y;
    auto dx = deformation.x;
    auto dy = deformation.y;
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

            float p00xVal = bx[idx00], p00yVal = by[idx00];
            float p10xVal = bx[idx10], p10yVal = by[idx10];
            float p01xVal = bx[idx01], p01yVal = by[idx01];
            float p11xVal = bx[idx11], p11yVal = by[idx11];
            if (includeDeformation) {
                p00xVal += dx[idx00]; p00yVal += dy[idx00];
                p10xVal += dx[idx10]; p10yVal += dy[idx10];
                p01xVal += dx[idx01]; p01yVal += dy[idx01];
                p11xVal += dx[idx11]; p11yVal += dy[idx11];
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
        float p00xVal = bx[idx00], p00yVal = by[idx00];
        float p10xVal = bx[idx10], p10yVal = by[idx10];
        float p01xVal = bx[idx01], p01yVal = by[idx01];
        float p11xVal = bx[idx11], p11yVal = by[idx11];
        if (includeDeformation) {
            p00xVal += dx[idx00]; p00yVal += dy[idx00];
            p10xVal += dx[idx10]; p10yVal += dy[idx10];
            p01xVal += dx[idx01]; p01yVal += dy[idx01];
            p11xVal += dx[idx11]; p11yVal += dy[idx11];
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

DeformResult GridDeformer::deformChildren(const std::shared_ptr<Node>& target,
                                          const std::vector<Vec2>& origVertices,
                                          const std::vector<Vec2>& origDeformation,
                                          const Mat4* origTransform) {
    static std::unordered_map<uint32_t, uint64_t> sChangedCount;
    static uint64_t sSummaryTick = 0;
    DeformResult res;
    res.vertices = origDeformation.empty() ? origVertices : origDeformation;
    res.transform = std::nullopt;
    res.changed = false;

    if (!hasValidGrid() || !origTransform) return res;
    if (origDeformation.size() && origDeformation.size() < origVertices.size()) return res;
    if (auto path = std::dynamic_pointer_cast<PathDeformer>(target)) {
        if (!path->physicsEnabled) return res;
    }
    if (!matrixIsFinite(inverseMatrix) || !matrixIsFinite(*origTransform)) return res;

    Mat4 centerMatrix = Mat4::multiply(inverseMatrix, *origTransform);
    if (!matrixIsFinite(centerMatrix)) return res;

    Vec2Array samplePoints;
    transformAssign(samplePoints, toVec2Array(origVertices), centerMatrix);
    if (dynamic && !origDeformation.empty() && samplePoints.size()) {
        auto deformArr = toVec2Array(origDeformation);
        std::size_t overlap = std::min<std::size_t>(samplePoints.size(), deformArr.size());
        transformAdd(samplePoints, deformArr, centerMatrix, overlap);
    }

    std::vector<GridCellCache> caches(samplePoints.size());
    bool anyChanged = false;
    bool invalidSamples = false;
    for (std::size_t i = 0; i < samplePoints.size(); ++i) {
        caches[i] = computeCache(samplePoints.xAt(i), samplePoints.yAt(i));
        if (!caches[i].valid) {
            invalidSamples = true;
            break;
        }
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
    Vec2Array deformOut;
    deformOut.resize(std::max<std::size_t>(origDeformation.size(), offsetLocal.size()));
    for (std::size_t i = 0; i < origDeformation.size() && i < deformOut.size(); ++i) {
        deformOut.xAt(i) = origDeformation[i].x;
        deformOut.yAt(i) = origDeformation[i].y;
    }
    if (deformOut.size() < offsetLocal.size()) deformOut.resize(offsetLocal.size());
    transformAdd(deformOut, offsetLocal, inv, offsetLocal.size());

    res.vertices = toVec2List(deformOut);
    res.changed = true;
    if (traceDeformerSummaryEnabled() && target && res.changed) {
        sChangedCount[target->uuid] += 1;
        ++sSummaryTick;
        if ((sSummaryTick % 240) == 0) {
            std::fprintf(stderr, "[nicxlive][GridDeformer][Summary] changedTargets=%zu\n", sChangedCount.size());
            int printed = 0;
            for (const auto& kv : sChangedCount) {
                std::fprintf(stderr, "[nicxlive][GridDeformer][Summary] targetUuid=%u hits=%llu\n",
                             kv.first,
                             static_cast<unsigned long long>(kv.second));
                if (++printed >= 8) break;
            }
        }
    }
    return res;
}

void GridDeformer::runPreProcessTask(core::RenderContext& ctx) {
    Deformable::runPreProcessTask(ctx);
    // ensure transform cache is fresh before children read inverseMatrix
    Mat4 m = transform().toMat4();
    if (!matrixIsFinite(m)) return;
    inverseMatrix = m.inverse();
    updateDeform();
}

void GridDeformer::runRenderTask(core::RenderContext&) {
    // no GPU commands
}

bool GridDeformer::setupChild(const std::shared_ptr<Node>& child) {
    Deformable::setupChild(child);
    if (!child || !hasValidGrid()) return false;
    setupChildNoRecurse(child);
    if (child->mustPropagate()) {
        for (auto& c : child->childrenRef()) setupChild(c);
    }
    return false;
}

bool GridDeformer::releaseChild(const std::shared_ptr<Node>& child) {
    releaseChildNoRecurse(child);
    if (child && child->mustPropagate()) {
        for (auto& c : child->childrenRef()) releaseChild(c);
    }
    Deformable::releaseChild(child);
    return false;
}

void GridDeformer::captureTarget(const std::shared_ptr<Node>& target) {
    childrenRef().push_back(target);
    setupChildNoRecurse(target, true);
}

void GridDeformer::releaseTarget(const std::shared_ptr<Node>& target) {
    releaseChildNoRecurse(target);
    auto& ch = childrenRef();
    ch.erase(std::remove(ch.begin(), ch.end(), target), ch.end());
}

void GridDeformer::clearCache() {
    // no extra caches yet
}

void GridDeformer::build(bool force) {
    for (auto& c : children) setupChild(c);
    setupSelf();
    refresh();
}

void GridDeformer::setupChildNoRecurse(const std::shared_ptr<Node>& node, bool prepend) {
    if (!node) return;
    if (auto comp = std::dynamic_pointer_cast<Composite>(node)) {
        if (comp->propagateMeshGroupEnabled()) {
            releaseChildNoRecurse(node);
            return;
        }
    }
    auto deformable = std::dynamic_pointer_cast<Deformable>(node);
    bool isDeformable = static_cast<bool>(deformable);
    Node::FilterHook hook;
    hook.stage = kGridFilterStage;
    hook.tag = reinterpret_cast<std::uintptr_t>(this);
    hook.func = [this](auto t, auto v, auto d, auto mat) {
        auto res = deformChildren(t, v, d, mat);
        return std::make_tuple(res.vertices, std::optional<Mat4>{}, res.changed);
    };
    auto& pre = node->preProcessFilters;
    auto& post = node->postProcessFilters;
    const auto tag = reinterpret_cast<std::uintptr_t>(this);
    pre.erase(std::remove_if(pre.begin(), pre.end(), [&](const auto& h) { return h.stage == kGridFilterStage && h.tag == tag; }), pre.end());
    post.erase(std::remove_if(post.begin(), post.end(), [&](const auto& h) { return h.stage == kGridFilterStage && h.tag == tag; }), post.end());
    if (isDeformable) {
        if (dynamic) {
            if (prepend) post.insert(post.begin(), hook); else post.push_back(hook);
        } else {
            if (prepend) pre.insert(pre.begin(), hook); else pre.push_back(hook);
        }
    } else if (translateChildren) {
        if (prepend) pre.insert(pre.begin(), hook); else pre.push_back(hook);
    } else {
        releaseChildNoRecurse(node);
    }
}

void GridDeformer::releaseChildNoRecurse(const std::shared_ptr<Node>& node) {
    if (!node) return;
    auto& pre = node->preProcessFilters;
    auto& post = node->postProcessFilters;
    const auto tag = reinterpret_cast<std::uintptr_t>(this);
    pre.erase(std::remove_if(pre.begin(), pre.end(), [&](const auto& h) { return h.stage == kGridFilterStage && h.tag == tag; }), pre.end());
    post.erase(std::remove_if(post.begin(), post.end(), [&](const auto& h) { return h.stage == kGridFilterStage && h.tag == tag; }), post.end());
}

void GridDeformer::applyDeformToChildren(const std::vector<std::shared_ptr<core::param::Parameter>>& params, bool recursive) {
    switchDynamic(false);
    std::function<void(const std::shared_ptr<Node>&)> apply;
    apply = [&](const std::shared_ptr<Node>& c) {
        if (auto deformable = std::dynamic_pointer_cast<Deformable>(c)) {
            for (auto& param : params) {
                if (!param) continue;
                auto binding = std::dynamic_pointer_cast<core::param::DeformationParameterBinding>(param->getBinding(c, "deform"));
                if (!binding) continue;
                core::param::Vec2u left{};
                Vec2 sub{};
                param->findOffset(param->value, left, sub);
                auto slot = binding->sample(left, sub);
                if (slot.vertexOffsets.size() == deformable->deformation.size()) {
                    deformable->deformation = slot.vertexOffsets;
                }
            }
            auto m = c->transform().toMat4();
            auto res = deformChildren(c, toVec2List(deformable->vertices), toVec2List(deformable->deformation), &m);
            if (res.changed && res.vertices.size() == deformable->deformation.size()) {
                deformable->deformation = toVec2Array(res.vertices);
                deformable->transformChanged();
            }
        } else if (translateChildren) {
            // non-deformable: still propagate positional offset if allowed
            auto parentNode = c->parentPtr();
            Mat4 m = parentNode ? parentNode->transform().toMat4() : Mat4::identity();
            Vec2Array verts;
            verts.resize(1);
            verts.xAt(0) = c->localTransform.translation.x;
            verts.yAt(0) = c->localTransform.translation.y;
            Vec2Array deform;
            deform.resize(1);
            deform.xAt(0) = c->offsetTransform.translation.x;
            deform.yAt(0) = c->offsetTransform.translation.y;
            auto res = deformChildren(c, toVec2List(verts), toVec2List(deform), &m);
            if (res.changed && !res.vertices.empty()) {
                c->offsetTransform.translation.x = res.vertices[0].x;
                c->offsetTransform.translation.y = res.vertices[0].y;
                c->transformChanged();
            }
        }
        if (recursive && c->mustPropagate()) {
            for (auto& gc : c->childrenRef()) apply(gc);
        }
    };
    inverseMatrix = transform().toMat4().inverse();
    for (auto& c : children) apply(c);
}

void GridDeformer::serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const {
    Deformable::serializeSelfImpl(serializer, recursive, flags);
    if (has_flag(flags, SerializeNodeFlags::Geometry)) {
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
    }
    if (has_flag(flags, SerializeNodeFlags::State)) {
        serializer.putKey("formation");
        serializer.putValue(static_cast<int>(formation));
        serializer.putKey("dynamic");
        serializer.putValue(dynamic);
    }
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
    if (auto fm = data.get_child_optional("formation")) {
        formation = static_cast<GridFormation>(fm->get_value<int>());
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

std::vector<float> GridDeformer::normalizeAxis(const std::vector<float>& values) const {
    if (values.empty()) return values;
    std::vector<float> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end(), [](float a, float b) {
        return std::fabs(a - b) < kAxisTolerance;
    }), sorted.end());
    return sorted;
}

int GridDeformer::axisIndexOfValue(const std::vector<float>& axis, float value) const {
    for (std::size_t i = 0; i < axis.size(); ++i) {
        if (std::fabs(axis[i] - value) < kAxisTolerance) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool GridDeformer::deriveAxes(const Vec2Array& points, std::vector<float>& xs, std::vector<float>& ys) const {
    if (points.size() < 4) return false;
    xs.assign(points.x.begin(), points.x.end());
    ys.assign(points.y.begin(), points.y.end());
    xs = normalizeAxis(xs);
    ys = normalizeAxis(ys);
    if (xs.size() < 2 || ys.size() < 2) return false;
    if (xs.size() * ys.size() != points.size()) return false;

    std::vector<bool> seen(xs.size() * ys.size(), false);
    for (std::size_t i = 0; i < points.size(); ++i) {
        int xi = axisIndexOfValue(xs, points.xAt(i));
        int yi = axisIndexOfValue(ys, points.yAt(i));
        if (xi < 0 || yi < 0) return false;
        auto idx = static_cast<std::size_t>(yi) * xs.size() + static_cast<std::size_t>(xi);
        seen[idx] = true;
    }
    for (bool f : seen) if (!f) return false;
    return true;
}

bool GridDeformer::fillDeformationFromPositions(const Vec2Array& positions) {
    if (positions.size() != deformation.size()) {
        deformation.fill(Vec2{0.0f, 0.0f});
        return false;
    }
    std::vector<bool> seen(deformation.size(), false);
    for (std::size_t i = 0; i < positions.size(); ++i) {
        int xi = axisIndexOfValue(axisX, positions.xAt(i));
        int yi = axisIndexOfValue(axisY, positions.yAt(i));
        if (xi < 0 || yi < 0) {
            deformation.fill(Vec2{0.0f, 0.0f});
            return false;
        }
        auto idx = gridIndex(static_cast<std::size_t>(xi), static_cast<std::size_t>(yi));
        deformation.set(idx, Vec2{
            positions.xAt(i) - vertices.xAt(idx),
            positions.yAt(i) - vertices.yAt(idx),
        });
        seen[idx] = true;
    }
    for (bool f : seen) {
        if (!f) {
            deformation.fill(Vec2{0.0f, 0.0f});
            return false;
        }
    }
    return true;
}

bool GridDeformer::adoptFromVertices(const Vec2Array& points, bool preserveShape) {
    std::vector<float> xs;
    std::vector<float> ys;
    if (!deriveAxes(points, xs, ys)) return false;
    setGridAxes(xs, ys);
    if (preserveShape) {
        if (!fillDeformationFromPositions(points)) return false;
    }
    return true;
}

void GridDeformer::adoptGridFromAxes(const std::vector<float>& xs, const std::vector<float>& ys) {
    setGridAxes(xs, ys);
}

GridDeformer::IntervalResult GridDeformer::locateInterval(const std::vector<float>& axis, float value) const {
    IntervalResult r;
    if (axis.size() < 2) return r;
    if (value < axis.front() || value > axis.back()) return r;
    std::size_t idx = axis.size() - 2;
    for (std::size_t i = 0; i + 1 < axis.size(); ++i) {
        if (value <= axis[i + 1]) {
            idx = i;
            break;
        }
    }
    float span = axis[idx + 1] - axis[idx];
    r.index = idx;
    r.weight = span > 0 ? (value - axis[idx]) / span : 0.0f;
    r.valid = true;
    return r;
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
    Deformable::copyFrom(src, clone, deepCopy);
    if (auto g = dynamic_cast<const GridDeformer*>(&src)) {
        axisX = g->axisX;
        axisY = g->axisY;
        formation = g->formation;
        dynamic = g->dynamic;
        translateChildren = g->translateChildren;
        inverseMatrix = g->inverseMatrix;
        setGridAxes(axisX, axisY);
        deformation = g->deformation;
    } else if (auto drawable = dynamic_cast<const Deformable*>(&src)) {
        if (!adoptFromVertices(drawable->vertices, true)) {
            adoptGridFromAxes(std::vector<float>(kDefaultAxis.begin(), kDefaultAxis.end()),
                              std::vector<float>(kDefaultAxis.begin(), kDefaultAxis.end()));
        }
    } else {
        adoptGridFromAxes(std::vector<float>(kDefaultAxis.begin(), kDefaultAxis.end()),
                          std::vector<float>(kDefaultAxis.begin(), kDefaultAxis.end()));
    }
}

} // namespace nicxlive::core::nodes
