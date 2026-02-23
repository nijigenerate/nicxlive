#include "path_deformer.hpp"

#include "../common/utils.hpp"
#include "../render.hpp"
#include "../puppet.hpp"
#include "../serde.hpp"
#include "../param/parameter.hpp"
#include "curve.hpp"
#include "deformer/drivers/phys.hpp"
#include "drawable.hpp"
#include "grid_deformer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <unordered_map>

namespace nicxlive::core::nodes {

using nicxlive::core::common::transformAssign;
using nicxlive::core::common::transformAdd;
using nicxlive::core::common::gatherVec2;
using nicxlive::core::common::scatterAddVec2;

namespace {
constexpr int kPathFilterStage = 1;
constexpr std::size_t kInvalidDisableThreshold = 10;
constexpr std::size_t kInvalidLogInterval = 10;
constexpr std::size_t kInvalidLogFrameInterval = 30;
constexpr std::size_t kInvalidInitialLogAllowance = 3;

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

template <typename... Args>
void pathLog(const Args&... args) {
    // simple debug logger
    (std::cerr << ... << args) << std::endl;
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

int firstNonFiniteIndex(const Vec2Array& data) {
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (!std::isfinite(data.xAt(i)) || !std::isfinite(data.yAt(i))) return static_cast<int>(i);
    }
    return -1;
}

void normalizeVec2Array(Vec2Array& data, float epsilon, const Vec2& fallback) {
    for (std::size_t i = 0; i < data.size(); ++i) {
        float lenSq = data.xAt(i) * data.xAt(i) + data.yAt(i) * data.yAt(i);
        if (lenSq > epsilon) {
            float invLen = 1.0f / std::sqrt(lenSq);
            data.xAt(i) *= invLen;
            data.yAt(i) *= invLen;
        } else {
            data.xAt(i) = fallback.x;
            data.yAt(i) = fallback.y;
        }
    }
}

void normalizeVec2Array(Vec2Array& data, float epsilon, const Vec2Array& fallback) {
    for (std::size_t i = 0; i < data.size(); ++i) {
        float lenSq = data.xAt(i) * data.xAt(i) + data.yAt(i) * data.yAt(i);
        if (lenSq > epsilon) {
            float invLen = 1.0f / std::sqrt(lenSq);
            data.xAt(i) *= invLen;
            data.yAt(i) *= invLen;
        } else if (i < fallback.size()) {
            data.xAt(i) = fallback.xAt(i);
            data.yAt(i) = fallback.yAt(i);
        }
    }
}

void rotateTangentsToNormals(Vec2Array& dst, const Vec2Array& tangents) {
    dst.resize(tangents.size());
    for (std::size_t i = 0; i < tangents.size(); ++i) {
        dst.xAt(i) = -tangents.yAt(i);
        dst.yAt(i) = tangents.xAt(i);
    }
}

void projectVec2OntoAxes(const Vec2Array& points,
                         const Vec2Array& origins,
                         const Vec2Array& normals,
                         const Vec2Array& tangents,
                         std::vector<float>& normalDistances,
                         std::vector<float>& tangentialDistances) {
    std::size_t count = std::min({points.size(), origins.size(), normals.size(), tangents.size()});
    normalDistances.resize(count);
    tangentialDistances.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        float dx = points.xAt(i) - origins.xAt(i);
        float dy = points.yAt(i) - origins.yAt(i);
        normalDistances[i] = dx * normals.xAt(i) + dy * normals.yAt(i);
        tangentialDistances[i] = dx * tangents.xAt(i) + dy * tangents.yAt(i);
    }
}

void composeVec2FromAxes(Vec2Array& dst,
                         const Vec2Array& origins,
                         const std::vector<float>& normalDistances,
                         const Vec2Array& normals,
                         const std::vector<float>& tangentialDistances,
                         const Vec2Array& tangents) {
    std::size_t count = std::min({origins.size(), normals.size(), tangents.size(),
                                  normalDistances.size(), tangentialDistances.size()});
    dst.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        float nx = normals.xAt(i) * normalDistances[i];
        float ny = normals.yAt(i) * normalDistances[i];
        float tx = tangents.xAt(i) * tangentialDistances[i];
        float ty = tangents.yAt(i) * tangentialDistances[i];
        dst.xAt(i) = origins.xAt(i) + nx + tx;
        dst.yAt(i) = origins.yAt(i) + ny + ty;
    }
}

float computeCurveScale(const Vec2Array& pts) {
    float scale = 0.0f;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        float x = pts.xAt(i);
        float y = pts.yAt(i);
        if (!std::isfinite(x) || !std::isfinite(y)) continue;
        float mag = std::sqrt(x * x + y * y);
        if (mag > scale) scale = mag;
    }
    return scale;
}

std::string summarizePoints(const Vec2Array& pts, std::size_t maxCount = 4) {
    std::stringstream ss;
    ss << "len=" << pts.size() << " [";
    std::size_t limit = std::min<std::size_t>(maxCount, pts.size());
    for (std::size_t i = 0; i < limit; ++i) {
        ss << "(" << pts.xAt(i) << "," << pts.yAt(i) << ")";
        if (i + 1 < limit) ss << ", ";
    }
    if (pts.size() > maxCount) ss << ", ...";
    ss << "]";
    return ss.str();
}

bool approxEqual(float a, float b, float eps = 1e-3f) {
    if (std::isnan(a) && std::isnan(b)) return true;
    return std::fabs(a - b) <= eps;
}
} // namespace

PathDeformer::PathDeformer() {
    requirePreProcessTask();
    diagnosticsEnabled = false;
    originalCurve = createCurve(Vec2Array{}, curveType == CurveType::Bezier);
    deformedCurve = createCurve(Vec2Array{}, curveType == CurveType::Bezier);
    prevCurve = createCurve(Vec2Array{}, curveType == CurveType::Bezier);
    driver.reset();
    prevRootSet = false;
}

void PathDeformer::ConnectedDriverAdapter::setup() {
    if (!impl_ || !owner_) return;
    impl_->setup(owner_);
}

void PathDeformer::ConnectedDriverAdapter::update(Vec2Array& offsets, float strength, uint64_t frame) {
    if (!impl_ || !owner_) return;
    impl_->update(owner_, offsets);
}

void PathDeformer::ConnectedDriverAdapter::updateDefaultShape() {
    if (!impl_ || !owner_) return;
    impl_->updateDefaultShape(owner_);
}

void PathDeformer::ConnectedDriverAdapter::reset() {
    if (!impl_) return;
    impl_->reset();
}

void PathDeformer::ConnectedDriverAdapter::enforce(const Vec2& f) {
    if (!impl_) return;
    impl_->enforce(f);
}

void PathDeformer::ConnectedDriverAdapter::rotate(float a) {
    if (!impl_) return;
    impl_->rotate(a);
}

void PathDeformer::ensureDriver() {
    if (hasDegenerateBaseline) {
        driver.reset();
        return;
    }
    if (!driver) {
        driver = createPhysicsDriver();
    }
}

std::unique_ptr<PathDeformer::PhysicsDriver> PathDeformer::createPhysicsDriver() {
    if (hasDegenerateBaseline) {
        pathLog("[PathDeformer][PhysicsDisabled] reason=degenerateBaseline");
        return nullptr;
    }
    switch (physicsType) {
    case PhysicsType::SpringPendulum:
        return std::make_unique<ConnectedDriverAdapter>(std::make_unique<ConnectedSpringPendulumDriver>(this), this);
    case PhysicsType::Pendulum:
    default:
        return std::make_unique<ConnectedDriverAdapter>(std::make_unique<ConnectedPendulumDriver>(this), this);
    }
}

void PathDeformer::rebuildCurveSamples() {
    curveSamples.clear();
    curveNormals.clear();
    curveParams.clear();
    validateCurve();
    if (controlPoints.size() < 2) return;
    const float step = 0.05f;
    if (curveType == CurveType::Bezier && controlPoints.size() >= 3) {
        for (std::size_t i = 0; i + 2 < controlPoints.size(); i += 2) {
            Vec2 p0 = controlPoints[i];
            Vec2 p1 = controlPoints[i + 1];
            Vec2 p2 = controlPoints[i + 2];
            for (float t = 0.0f; t <= 1.0f; t += step) {
                float u = 1.0f - t;
                float b0 = u * u;
                float b1 = 2 * u * t;
                float b2 = t * t;
                Vec2 pos{b0 * p0.x + b1 * p1.x + b2 * p2.x,
                         b0 * p0.y + b1 * p1.y + b2 * p2.y};
                // derivative for normal
                float dx = 2 * u * (p1.x - p0.x) + 2 * t * (p2.x - p1.x);
                float dy = 2 * u * (p1.y - p0.y) + 2 * t * (p2.y - p1.y);
                float len = std::sqrt(dx * dx + dy * dy);
                Vec2 n{0, 0};
                if (len > 1e-6f) { n.x = -dy / len; n.y = dx / len; }
                curveSamples.push_back(pos);
                curveNormals.push_back(n);
                curveParams.push_back(static_cast<float>(curveSamples.size()) * step);
            }
        }
    } else {
        for (std::size_t i = 0; i + 1 < controlPoints.size(); ++i) {
            Vec2 a = controlPoints[i];
            Vec2 b = controlPoints[i + 1];
            for (float t = 0.0f; t <= 1.0f; t += step) {
                Vec2 pos{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
                float dx = b.x - a.x;
                float dy = b.y - a.y;
                float len = std::sqrt(dx * dx + dy * dy);
                Vec2 n{0, 0};
                if (len > 1e-6f) { n.x = -dy / len; n.y = dx / len; }
                curveSamples.push_back(pos);
                curveNormals.push_back(n);
                curveParams.push_back(static_cast<float>(curveSamples.size()) * step);
            }
        }
    }
}

Vec2 PathDeformer::projectToCurve(const Vec2& p) const {
    const std::vector<Vec2>& pts = !curveSamples.empty() ? curveSamples : controlPoints;
    if (pts.size() < 2) return p;
    float bestDist = std::numeric_limits<float>::max();
    Vec2 best{p.x, p.y};
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        Vec2 a = pts[i];
        Vec2 b = pts[i + 1];
        float abx = b.x - a.x;
        float aby = b.y - a.y;
        float denom = abx * abx + aby * aby;
        float t = denom > 0 ? ((p.x - a.x) * abx + (p.y - a.y) * aby) / denom : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);
        Vec2 proj{a.x + abx * t, a.y + aby * t};
        float dx = proj.x - p.x;
        float dy = proj.y - p.y;
        float dist = dx * dx + dy * dy;
        if (dist < bestDist) {
            bestDist = dist;
            best = proj;
        }
    }
    return best;
}

Vec2 PathDeformer::evaluateCurve(float t) const {
    if (controlPoints.empty()) return Vec2{};
    if (curveType == CurveType::Bezier && controlPoints.size() >= 3) {
        // normalize t over segments of 2 control points (quadratic)
        float segCount = static_cast<float>((controlPoints.size() - 1) / 2);
        if (segCount <= 0) return controlPoints.front();
        float seg = std::clamp(t, 0.0f, 0.999f) * segCount;
        int idx = static_cast<int>(std::floor(seg));
        float lt = seg - idx;
        int i0 = idx * 2;
        int i1 = i0 + 1;
        int i2 = i0 + 2;
        i2 = std::min(i2, static_cast<int>(controlPoints.size() - 1));
        Vec2 p0 = controlPoints[i0];
        Vec2 p1 = controlPoints[i1];
        Vec2 p2 = controlPoints[i2];
        float u = 1.0f - lt;
        float b0 = u * u;
        float b1 = 2 * u * lt;
        float b2 = lt * lt;
        return Vec2{b0 * p0.x + b1 * p1.x + b2 * p2.x,
                    b0 * p0.y + b1 * p1.y + b2 * p2.y};
    } else {
        // linear spline
        if (controlPoints.size() == 1) return controlPoints.front();
        float segCount = static_cast<float>(controlPoints.size() - 1);
        float seg = std::clamp(t, 0.0f, 0.999f) * segCount;
        int idx = static_cast<int>(std::floor(seg));
        float lt = seg - idx;
        Vec2 a = controlPoints[idx];
        Vec2 b = controlPoints[std::min(idx + 1, static_cast<int>(controlPoints.size() - 1))];
        return Vec2{a.x + (b.x - a.x) * lt, a.y + (b.y - a.y) * lt};
    }
}

Vec2 PathDeformer::evaluateTangent(float t) const {
    if (controlPoints.size() < 2) return Vec2{};
    Vec2 a = evaluateCurve(std::max(0.0f, t - 0.01f));
    Vec2 b = evaluateCurve(std::min(1.0f, t + 0.01f));
    Vec2 tan{b.x - a.x, b.y - a.y};
    float len = std::sqrt(tan.x * tan.x + tan.y * tan.y);
    if (len > 1e-6f) { tan.x /= len; tan.y /= len; }
    return tan;
}

float PathDeformer::curveLength() const {
    float len = 0.0f;
    const auto& pts = !curveSamples.empty() ? curveSamples : controlPoints;
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        float dx = pts[i + 1].x - pts[i].x;
        float dy = pts[i + 1].y - pts[i].y;
        len += std::sqrt(dx * dx + dy * dy);
    }
    return len;
}

void PathDeformer::cacheClosestPoints(const std::shared_ptr<Node>& node, int nSamples) {
    if (!node) return;
    Vec2Array nodeVertices;
    if (auto deformable = std::dynamic_pointer_cast<Deformable>(node)) {
        nodeVertices.resize(deformable->vertices.size());
        for (std::size_t i = 0; i < deformable->vertices.size(); ++i) {
            nodeVertices.xAt(i) = deformable->vertices[i].x;
            nodeVertices.yAt(i) = deformable->vertices[i].y;
        }
    } else {
        nodeVertices.resize(1);
        auto t = node->transform();
        nodeVertices.xAt(0) = t.translation.x;
        nodeVertices.yAt(0) = t.translation.y;
    }

    auto& cache = meshCaches[node->uuid];
    cache.resize(nodeVertices.size());

    const auto& curve = prevCurve ? prevCurve : originalCurve;
    if (!curve) return;

    Mat4 forward = node->globalTransform.toMat4();
    Mat4 inv = Mat4::inverse(globalTransform.toMat4());
    Mat4 tran = Mat4::multiply(inv, forward);

    Vec2Array cVertices;
    transformAssign(cVertices, nodeVertices, tran);
    for (std::size_t i = 0; i < cVertices.size(); ++i) {
        cache[i] = curve->closestPoint(Vec2{cVertices.xAt(i), cVertices.yAt(i)}, nSamples);
    }
}

void PathDeformer::recordInvalid(const std::string& ctx, std::size_t idx, const Vec2& value) {
    ensureDiagnosticCapacity(idx + 1);
    invalidThisFrame = true;
    diagnostics.invalidThisFrame = true;
    if (!diagnosticsEnabled) return;
    invalidIndexThisFrame[idx] = true;
    if (invalidStreakStartFrame[idx] == 0) invalidStreakStartFrame[idx] = frameCounter;
    invalidPerIndex[idx]++;
    if (frameCounter == invalidLastFrame[idx] + 1) {
        invalidConsecutive[idx]++;
    } else {
        invalidConsecutive[idx] = 1;
    }
    invalidLastFrame[idx] = frameCounter;
    invalidLastValue[idx] = value;
    diagnostics.invalidTotalPerIndex[idx] = invalidPerIndex[idx];
    diagnostics.invalidConsecutivePerIndex[idx] = invalidConsecutive[idx];
    diagnostics.invalidIndexThisFrame[idx] = invalidIndexThisFrame[idx];
    diagnostics.invalidStreakStartFrame[idx] = invalidStreakStartFrame[idx];
    diagnostics.invalidLastLoggedFrame[idx] = invalidLastLoggedFrame[idx];
    diagnostics.invalidLastLoggedCount[idx] = invalidLastLoggedCount[idx];
    diagnostics.invalidLastLoggedValueWasNaN[idx] = invalidLastLoggedValueWasNaN[idx];
    diagnostics.invalidLastLoggedValue[idx] = invalidLastLoggedValue[idx];
    diagnostics.invalidLastLoggedContext[idx] = invalidLastLoggedContext[idx];
    diagnostics.totalInvalidCount = totalInvalidCount;
    lastInvalidContext = ctx;
    std::stringstream ss;
    ss << "[PathDeformer][Invalid] frame=" << frameCounter << " idx=" << idx
       << " ctx=" << ctx << " val=(" << value.x << "," << value.y << ")";
    pathLog(ss.str());
    invalidLog.push_back(InvalidRecord{frameCounter, ctx, value});
}

void PathDeformer::logDiagnostics() const {
    if (!diagnosticsEnabled) return;
    std::stringstream ss;
    ss << "[PathDeformer][Diag] frame=" << frameCounter
       << " invalidFrames=" << invalidFrameCount
       << " totalInvalid=" << totalInvalidCount
       << " lastCtx=" << lastInvalidContext
       << " maxOffset=" << lastMaxOffset
       << " avgOffset=" << lastAvgOffset;
    pathLog(ss.str());
    if (preserveInvalidLog) {
        for (const auto& rec : invalidLog) {
            std::stringstream ls;
            ls << "[PathDeformer][DiagLog] frame=" << rec.frame << " ctx=" << rec.context
               << " val=(" << rec.value.x << "," << rec.value.y << ")";
            pathLog(ls.str());
        }
    }
}

void PathDeformer::sanitizeOffsets(Vec2Array& offsets) {
    for (std::size_t i = 0; i < offsets.size(); ++i) {
        if (!std::isfinite(offsets.xAt(i)) || !std::isfinite(offsets.yAt(i))) {
            offsets.xAt(i) = 0.0f;
            offsets.yAt(i) = 0.0f;
            recordInvalid("sanitize", i, Vec2{0, 0});
        }
    }
}

void PathDeformer::validateCurve() {
    if (!diagnosticsEnabled) return;
    if (controlPoints.size() < 2) {
        recordInvalid("curvePoints", 0, Vec2{});
        return;
    }
    for (std::size_t i = 0; i + 1 < controlPoints.size(); ++i) {
        Vec2 a = controlPoints[i];
        Vec2 b = controlPoints[i + 1];
        if (a.x == b.x && a.y == b.y) {
            recordInvalid("degenerateSeg", i, a);
        }
    }
}

void PathDeformer::resetDiagnostics() {
    invalidThisFrame = false;
    lastInvalidContext.clear();
    invalidFrameCount = 0;
    totalInvalidCount = 0;
    invalidPerIndex.clear();
    invalidConsecutive.clear();
    invalidLastFrame.clear();
    invalidLastValue.clear();
    if (!preserveInvalidLog) invalidLog.clear();
    matrixInvalidThisFrame = false;
    consecutiveInvalidFrames = 0;
    diagnostics = DiagnosticsState{};
}

bool beginDiagnosticFrameFlag(bool& flag) {
    bool was = flag;
    flag = true;
    return !was;
}

void endDiagnosticFrameFlag(bool& flag) { flag = false; }

void PathDeformer::disablePhysicsDriver(const std::string& reason) {
    if (!driver) return;
    driverInitialized = false;
    prevRootSet = false;
    physicsOnly = false;
    deformation.fill(Vec2{0.0f, 0.0f});
    driver->reset();
}

Vec2 PathDeformer::sanitizeVec2(const Vec2& v) const {
    if (std::isfinite(v.x) && std::isfinite(v.y)) return v;
    return Vec2{0, 0};
}

void PathDeformer::logCurveState(const std::string& ctx) {
    if (!diagnosticsEnabled) return;
    std::stringstream ss;
    ss << "[PathDeformer][CurveDiag] ctx=" << ctx << " frame=" << frameCounter;
    pathLog(ss.str());
}

void PathDeformer::logCurveHealth(const std::string& ctx, const std::unique_ptr<Curve>& a, const std::unique_ptr<Curve>& b, const Vec2Array& def) {
    if (!diagnosticsEnabled) return;
    Vec2Array refPts;
    Vec2Array dstPts;
    std::vector<float> samples(def.size() > 0 ? def.size() : 1, 0.0f);
    for (std::size_t i = 0; i < samples.size(); ++i) samples[i] = static_cast<float>(i) / std::max<std::size_t>(1, samples.size() - 1);
    if (a) a->evaluatePoints(samples, refPts);
    if (b) b->evaluatePoints(samples, dstPts);
    bool hasNaN = firstNonFiniteIndex(dstPts) >= 0;
    float targetScale = computeCurveScale(dstPts);
    float refScale = computeCurveScale(refPts);
    bool collapsed = approxEqual(targetScale, 0.0f);
    bool refCollapsed = approxEqual(refScale, 0.0f);
    pathLog("[PathDeformer][CurveDiag] ctx=", ctx,
            " frame=", frameCounter,
            " defSize=", def.size(),
            " targetScale=", targetScale,
            " refScale=", refScale,
            " targetNaN=", hasNaN,
            " collapsed=", collapsed,
            " refCollapsed=", refCollapsed,
            " samples=", summarizePoints(dstPts));
    if (curveDiagTargetScale.empty()) {
        curveDiagTargetScale.push_back(targetScale);
        curveDiagReferenceScale.push_back(refScale);
        curveDiagHasNaN.push_back(hasNaN);
        curveDiagCollapsed.push_back(collapsed);
    } else {
        curveDiagTargetScale[0] = targetScale;
        curveDiagReferenceScale[0] = refScale;
        curveDiagHasNaN[0] = hasNaN;
        curveDiagCollapsed[0] = collapsed;
    }
}

Mat4 requireFiniteMatrix(const Mat4& m, const std::string& ctx, bool& ok) {
    if (!isFiniteMatrix(m)) {
        ok = false;
    }
    return m;
}

bool guardFinite(const Vec2& v) {
    return std::isfinite(v.x) && std::isfinite(v.y);
}

bool PathDeformer::beginDiagnosticFrame() {
    if (diagnosticsFrameActive) return false;
    diagnosticsFrameActive = true;
    diagnostics.diagnosticsFrameActive = true;
    ++frameCounter;
    ensureDiagnosticCapacity(deformation.size());
    clearInvalidFlags();
    diagnostics.invalidThisFrame = false;
    return true;
}

void PathDeformer::endDiagnosticFrame() {
    if (!diagnosticsFrameActive) return;
    if (invalidThisFrame || matrixInvalidThisFrame) {
        ++invalidFrameCount;
        ++consecutiveInvalidFrames;
    } else {
        consecutiveInvalidFrames = 0;
    }
    for (std::size_t i = 0; i < invalidConsecutive.size(); ++i) {
        bool flagged = i < invalidIndexThisFrame.size() ? invalidIndexThisFrame[i] : false;
        if (!flagged && invalidConsecutive[i] > 0) {
            pathLog("[PathDeformer][InvalidDeformationRecovered] node=", name,
                    " index=", i,
                    " frame=", frameCounter,
                    " lastedFrames=", invalidConsecutive[i],
                    " totalInvalid=", invalidPerIndex[i],
                    " firstFrame=", invalidStreakStartFrame[i]);
            invalidConsecutive[i] = 0;
            if (i < invalidStreakStartFrame.size()) invalidStreakStartFrame[i] = 0;
            if (i < invalidLastLoggedFrame.size()) invalidLastLoggedFrame[i] = 0;
            if (i < invalidLastLoggedCount.size()) invalidLastLoggedCount[i] = 0;
            if (i < invalidLastLoggedContext.size()) invalidLastLoggedContext[i].clear();
            if (i < invalidLastLoggedValueWasNaN.size()) invalidLastLoggedValueWasNaN[i] = false;
            if (i < invalidLastLoggedValue.size()) invalidLastLoggedValue[i] = Vec2{};
        }
    }
    diagnosticsFrameActive = false;
    diagnostics.invalidFrameCount = invalidFrameCount;
    diagnostics.totalInvalidCount = totalInvalidCount;
    diagnostics.invalidThisFrame = invalidThisFrame;
    diagnostics.diagnosticsFrameActive = false;
}

void PathDeformer::logTransformFailure(const std::string& ctx, const Mat4& m) {
    if (!diagnosticsEnabled) return;
    std::stringstream ss;
    ss << "[PathDeformer][TransformDiag] ctx=" << ctx << " mat=" << matrixSummary(m);
    pathLog(ss.str());
}

void PathDeformer::refreshInverseMatrix(const std::string& ctx) {
    bool okGlobal = true;
    Mat4 global = requireFiniteMatrix(globalTransform.toMat4(), ctx + ":global", okGlobal);
    bool okInv = true;
    Mat4 inv = requireFiniteMatrix(Mat4::inverse(global), ctx + ":inverse", okInv);
    if (!okGlobal || !okInv) {
        logTransformFailure(ctx, global);
        matrixInvalidThisFrame = true;
        invalidThisFrame = true;
    }
    inverseMatrix = inv;
}

void PathDeformer::markInvalidOffset(const std::string& ctx, std::size_t idx, const Vec2& value) {
    recordInvalid(ctx, idx, value);
    if (!diagnosticsEnabled) return;
    bool emit = shouldEmitInvalidIndexLog(idx, ctx, value, idx < invalidConsecutive.size() ? invalidConsecutive[idx] : 0);
    if (emit) logInvalidIndex(ctx, idx, value, idx < invalidConsecutive.size() ? invalidConsecutive[idx] : 0);
    if (idx < invalidConsecutive.size() && invalidConsecutive[idx] >= kInvalidDisableThreshold) {
        disablePhysicsDriver(ctx + ":threshold");
    }
}

void PathDeformer::logInvalidSnapshot(const std::string& ctx, const Vec2Array& deform) {
    if (!diagnosticsEnabled) return;
    std::stringstream ss;
    ss << "[PathDeformer][InvalidSnapshot] ctx=" << ctx << " count=" << deform.size();
    pathLog(ss.str());
}

void PathDeformer::logCurveDiag(const std::string& ctx, const Vec2Array& orig, const Vec2Array& deform) {
    if (!diagnosticsEnabled) return;
    std::stringstream ss;
    ss << "[PathDeformer][CurveDiag] ctx=" << ctx << " orig=" << orig.size() << " deform=" << deform.size();
    pathLog(ss.str());
}

void PathDeformer::reportInvalid(const std::string& ctx, std::size_t idx, const Vec2& value) {
    recordInvalid(ctx, idx, value);
}

void PathDeformer::ensureDiagnosticCapacity(std::size_t n) {
    if (invalidPerIndex.size() < n) invalidPerIndex.resize(n, 0);
    if (invalidConsecutive.size() < n) invalidConsecutive.resize(n, 0);
    if (invalidLastFrame.size() < n) invalidLastFrame.resize(n, 0);
    if (invalidLastValue.size() < n) invalidLastValue.resize(n, Vec2{});
    if (invalidIndexThisFrame.size() < n) invalidIndexThisFrame.resize(n, false);
    if (invalidStreakStartFrame.size() < n) invalidStreakStartFrame.resize(n, 0);
    if (invalidLastLoggedFrame.size() < n) invalidLastLoggedFrame.resize(n, 0);
    if (invalidLastLoggedCount.size() < n) invalidLastLoggedCount.resize(n, 0);
    if (invalidLastLoggedValueWasNaN.size() < n) invalidLastLoggedValueWasNaN.resize(n, false);
    if (curveDiagTargetScale.size() < n) curveDiagTargetScale.resize(n, 0.0f);
    if (curveDiagReferenceScale.size() < n) curveDiagReferenceScale.resize(n, 0.0f);
    if (curveDiagHasNaN.size() < n) curveDiagHasNaN.resize(n, false);
    if (curveDiagCollapsed.size() < n) curveDiagCollapsed.resize(n, false);
    if (invalidLastLoggedValue.size() < n) invalidLastLoggedValue.resize(n, Vec2{});
    if (invalidLastLoggedContext.size() < n) invalidLastLoggedContext.resize(n, std::string{});
    if (diagnostics.invalidTotalPerIndex.size() < n) diagnostics.invalidTotalPerIndex.resize(n, 0);
    if (diagnostics.invalidConsecutivePerIndex.size() < n) diagnostics.invalidConsecutivePerIndex.resize(n, 0);
    if (diagnostics.invalidIndexThisFrame.size() < n) diagnostics.invalidIndexThisFrame.resize(n, false);
    if (diagnostics.invalidStreakStartFrame.size() < n) diagnostics.invalidStreakStartFrame.resize(n, 0);
    if (diagnostics.invalidLastLoggedFrame.size() < n) diagnostics.invalidLastLoggedFrame.resize(n, 0);
    if (diagnostics.invalidLastLoggedCount.size() < n) diagnostics.invalidLastLoggedCount.resize(n, 0);
    if (diagnostics.invalidLastLoggedValueWasNaN.size() < n) diagnostics.invalidLastLoggedValueWasNaN.resize(n, false);
    if (diagnostics.invalidLastLoggedValue.size() < n) diagnostics.invalidLastLoggedValue.resize(n, Vec2{});
    if (diagnostics.invalidLastLoggedContext.size() < n) diagnostics.invalidLastLoggedContext.resize(n, std::string{});
}

void PathDeformer::checkBaselineDegeneracy(const std::vector<Vec2>& pts) {
    degenerateSegmentIndices.clear();
    hasDegenerateBaseline = false;
    for (std::size_t i = 1; i < pts.size(); ++i) {
        if (pts[i].x == pts[i - 1].x && pts[i].y == pts[i - 1].y) {
            degenerateSegmentIndices.push_back(i - 1);
            hasDegenerateBaseline = true;
        }
    }
    if (hasDegenerateBaseline && diagnosticsEnabled) {
        std::stringstream ss;
        ss << "[PathDeformer][DegenerateBaseline] segments=";
        for (std::size_t i = 0; i < degenerateSegmentIndices.size(); ++i) {
            ss << degenerateSegmentIndices[i];
            if (i + 1 < degenerateSegmentIndices.size()) ss << ",";
        }
        pathLog(ss.str());
    }
    if (hasDegenerateBaseline) {
        disablePhysicsDriver("degenerateBaseline");
    }
}

void PathDeformer::clearInvalidFlags() {
    std::fill(invalidIndexThisFrame.begin(), invalidIndexThisFrame.end(), false);
    matrixInvalidThisFrame = false;
    invalidThisFrame = false;
}

void PathDeformer::beginInvalidFrame() {
    clearInvalidFlags();
}

void PathDeformer::endInvalidFrame() {
    if (invalidThisFrame) {
        ++invalidFrameCount;
        ++totalInvalidCount;
    }
}

bool PathDeformer::shouldEmitInvalidIndexLog(std::size_t index, const std::string& context, const Vec2& value, std::size_t consecutive) {
    bool hasPrevLog = index < invalidLastLoggedFrame.size() && invalidLastLoggedFrame[index] != 0;
    bool valueIsNaN = !guardFinite(value);
    bool valueChanged = !hasPrevLog;
    if (hasPrevLog && index < invalidLastLoggedValue.size()) {
        const Vec2& prev = invalidLastLoggedValue[index];
        bool prevWasNaN = index < invalidLastLoggedValueWasNaN.size() && invalidLastLoggedValueWasNaN[index];
        if (prevWasNaN != valueIsNaN) {
            valueChanged = true;
        } else if (!valueIsNaN) {
            valueChanged = (prev.x != value.x) || (prev.y != value.y);
        }
    }
    bool contextChanged = !hasPrevLog || (index < invalidLastLoggedContext.size() && invalidLastLoggedContext[index] != context);
    bool consecutiveTrigger = (!hasPrevLog && consecutive >= 1 && invalidPerIndex[index] <= kInvalidInitialLogAllowance) ||
                              (consecutive == kInvalidDisableThreshold && index < invalidLastLoggedCount.size() && invalidLastLoggedCount[index] != consecutive) ||
                              (consecutive % kInvalidLogInterval == 0 && index < invalidLastLoggedCount.size() && invalidLastLoggedCount[index] != consecutive);
    bool timeTrigger = hasPrevLog && index < invalidLastLoggedFrame.size() && (frameCounter - invalidLastLoggedFrame[index]) >= kInvalidLogFrameInterval;
    bool totalTrigger = (invalidPerIndex[index] % kInvalidLogInterval == 0) && (!hasPrevLog || invalidLastLoggedFrame[index] != frameCounter);
    return valueChanged || contextChanged || consecutiveTrigger || timeTrigger || totalTrigger;
}

void PathDeformer::logInvalidIndex(const std::string& context, std::size_t index, const Vec2& value, std::size_t consecutive) {
    pathLog("[PathDeformer][InvalidDeformation] node=", name,
            " context=", context,
            " index=", index,
            " value=(", value.x, ",", value.y, ")",
            " frame=", frameCounter,
            " indexConsecutive=", consecutive,
            " indexTotal=", index < invalidPerIndex.size() ? invalidPerIndex[index] : 0,
            " driverActive=", driver != nullptr);
    if (index >= invalidLastLoggedFrame.size()) return;
    invalidLastLoggedFrame[index] = frameCounter;
    if (index < invalidLastLoggedCount.size()) invalidLastLoggedCount[index] = consecutive;
    if (index < invalidLastLoggedContext.size()) invalidLastLoggedContext[index] = context;
    if (index < invalidLastLoggedValue.size()) invalidLastLoggedValue[index] = value;
    if (index < invalidLastLoggedValueWasNaN.size()) invalidLastLoggedValueWasNaN[index] = !guardFinite(value);
}

DeformResult PathDeformer::deformChildren(const std::shared_ptr<Node>& target,
                                          const std::vector<Vec2>& origVertices,
                                          const std::vector<Vec2>& origDeformation,
                                          const Mat4* origTransform) {
    static std::unordered_map<uint32_t, uint64_t> sChangedCount;
    static uint64_t sSummaryTick = 0;
    DeformResult res;
    // D parity: unchanged path filter returns empty deformation payload.
    // Returning original vertices/deformation here causes caller-side overwrite.
    res.vertices.clear();
    res.transform = std::nullopt;
    res.changed = false;

    bool diagStarted = beginDiagnosticFrame();

    if (!origTransform) { if (diagStarted) endDiagnosticFrame(); return res; }
    if (!originalCurve || !deformedCurve) { if (diagStarted) endDiagnosticFrame(); return res; }
    if (controlPoints.size() < 2) { if (diagStarted) endDiagnosticFrame(); return res; }
    if (!origDeformation.empty() && origDeformation.size() < origVertices.size()) { if (diagStarted) endDiagnosticFrame(); return res; }
    if (!isFiniteMatrix(inverseMatrix) || !isFiniteMatrix(*origTransform)) {
        recordInvalid("inverseMatrix", 0, Vec2{});
        disablePhysicsDriver("inverseMatrix");
        if (diagStarted) endDiagnosticFrame();
        return res;
    }

    Mat4 center = Mat4::multiply(inverseMatrix, *origTransform);
    if (!isFiniteMatrix(center)) {
        recordInvalid("centerMatrix", 0, Vec2{});
        disablePhysicsDriver("centerMatrix");
        if (diagStarted) endDiagnosticFrame();
        return res;
    }

    Vec2Array verts;
    verts.resize(origVertices.size());
    for (std::size_t i = 0; i < origVertices.size(); ++i) {
        verts.xAt(i) = origVertices[i].x;
        verts.yAt(i) = origVertices[i].y;
    }

    Vec2Array deformInput;
    deformInput.resize(origDeformation.size());
    for (std::size_t i = 0; i < origDeformation.size(); ++i) {
        deformInput.xAt(i) = origDeformation[i].x;
        deformInput.yAt(i) = origDeformation[i].y;
    }
    sanitizeOffsets(deformInput);

    Vec2Array sample;
    transformAssign(sample, verts, center);
    if (!deformInput.empty()) {
        transformAdd(sample, deformInput, center, std::min(sample.size(), deformInput.size()));
    }

    int invalidCenter = firstNonFiniteIndex(sample);
    if (invalidCenter >= 0) {
        markInvalidOffset("cVertexNaN", static_cast<std::size_t>(invalidCenter), Vec2{});
        if (diagStarted) endDiagnosticFrame();
        return res;
    }

    // D parity: only reuse cached t-samples when entry already exists and has enough vertices.
    auto tgtId = target ? target->uuid : 0u;
    auto it = meshCaches.find(tgtId);
    bool hasValidCache = (it != meshCaches.end() && it->second.size() >= sample.size());
    if (!hasValidCache) {
        cacheClosestPoints(target);
        it = meshCaches.find(tgtId);
        hasValidCache = (it != meshCaches.end() && it->second.size() >= sample.size());
    }
    if (!hasValidCache) {
        if (diagStarted) endDiagnosticFrame();
        return res;
    }
    auto& cache = it->second;
    std::vector<float> tSamples(sample.size());
    std::copy_n(cache.begin(), sample.size(), tSamples.begin());

    Curve* baseCurve = prevCurve ? prevCurve.get() : originalCurve.get();
    Vec2Array closestOrig;
    Vec2Array closestDef;
    Vec2Array tangentOrigRaw;
    Vec2Array tangentDefRaw;
    if (baseCurve) baseCurve->evaluatePoints(tSamples, closestOrig);
    deformedCurve->evaluatePoints(tSamples, closestDef);
    if (baseCurve) baseCurve->evaluateDerivatives(tSamples, tangentOrigRaw);
    deformedCurve->evaluateDerivatives(tSamples, tangentDefRaw);

    int invalidDef = firstNonFiniteIndex(closestDef);
    if (invalidDef >= 0) {
        markInvalidOffset("closestPointDeformedNaN", static_cast<std::size_t>(invalidDef), Vec2{});
        if (diagStarted) endDiagnosticFrame();
        return res;
    }

    Vec2Array tangentOrig = tangentOrigRaw;
    normalizeVec2Array(tangentOrig, 1e-8f, Vec2{1.0f, 0.0f});
    Vec2Array tangentDef = tangentDefRaw;
    normalizeVec2Array(tangentDef, 1e-8f, tangentOrig);

    Vec2Array normalOrig;
    Vec2Array normalDef;
    rotateTangentsToNormals(normalOrig, tangentOrig);
    rotateTangentsToNormals(normalDef, tangentDef);

    std::vector<float> normalDistances;
    std::vector<float> tangentialDistances;
    projectVec2OntoAxes(sample, closestOrig, normalOrig, tangentOrig, normalDistances, tangentialDistances);

    Vec2Array deformedVertices;
    composeVec2FromAxes(deformedVertices, closestDef, normalDistances, normalDef, tangentialDistances, tangentDef);

    Vec2Array offsetLocal = deformedVertices;
    offsetLocal -= sample;

    bool anyChanged = false;
    float sumOffset = 0.0f;
    for (std::size_t i = 0; i < offsetLocal.size(); ++i) {
        if (!std::isfinite(offsetLocal.xAt(i)) || !std::isfinite(offsetLocal.yAt(i))) {
            offsetLocal.xAt(i) = 0;
            offsetLocal.yAt(i) = 0;
            recordInvalid("offsetNaN", i, Vec2{0, 0});
        } else if (offsetLocal.xAt(i) != 0 || offsetLocal.yAt(i) != 0) {
            anyChanged = true;
        }
        float mag = std::sqrt(offsetLocal.xAt(i) * offsetLocal.xAt(i) + offsetLocal.yAt(i) * offsetLocal.yAt(i));
        sumOffset += mag;
        lastMaxOffset = std::max(lastMaxOffset, mag);
    }
    if (offsetLocal.size() > 0) lastAvgOffset = sumOffset / static_cast<float>(offsetLocal.size());
    if (!anyChanged) { if (diagStarted) endDiagnosticFrame(); return res; }

    Mat4 inv = Mat4::inverse(center);
    if (!isFiniteMatrix(inv)) {
        recordInvalid("centerInverse", 0, Vec2{});
        disablePhysicsDriver("centerInverse");
        if (diagStarted) endDiagnosticFrame();
        return res;
    }
    inv[0][3] = inv[1][3] = inv[2][3] = 0.0f;
    Vec2Array deformOut;
    deformOut.resize(std::max<std::size_t>(deformInput.size(), offsetLocal.size()));
    for (std::size_t i = 0; i < deformInput.size() && i < deformOut.size(); ++i) {
        deformOut.xAt(i) = deformInput.xAt(i);
        deformOut.yAt(i) = deformInput.yAt(i);
    }
    transformAdd(deformOut, offsetLocal, inv, offsetLocal.size());

    res.vertices.resize(deformOut.size());
    float maxAbs = 0.0f;
    for (std::size_t i = 0; i < deformOut.size(); ++i) {
        res.vertices[i] = Vec2{deformOut.xAt(i), deformOut.yAt(i)};
        if (!std::isfinite(res.vertices[i].x) || !std::isfinite(res.vertices[i].y)) {
            recordInvalid("resultNaN", i, res.vertices[i]);
            res.vertices[i] = Vec2{0, 0};
        }
        maxAbs = std::max(maxAbs, std::max(std::fabs(res.vertices[i].x), std::fabs(res.vertices[i].y)));
    }
    res.changed = true;
    if (maxAbs > 10.0f) {
        pathLog("[PathDeformer][LargeOffset] node=", name,
                " target=", (target ? target->name : std::string("<null>")),
                " targetUuid=", (target ? target->uuid : 0u),
                " maxAbs=", maxAbs,
                " first=(", (res.vertices.empty() ? 0.0f : res.vertices[0].x),
                ",", (res.vertices.empty() ? 0.0f : res.vertices[0].y), ")");
    }
    if (driver && target) {
        target->notifyChange(target);
    }
    if (invalidThisFrame) {
        ++invalidFrameCount;
        ++consecutiveInvalidFrames;
    } else {
        consecutiveInvalidFrames = 0;
    }
    if (consecutiveInvalidFrames > 10) {
        disablePhysicsDriver("consecutiveInvalid");
    }
    if (diagStarted) endDiagnosticFrame();
    if (traceDeformerSummaryEnabled() && target && res.changed) {
        sChangedCount[target->uuid] += 1;
        ++sSummaryTick;
        if ((sSummaryTick % 240) == 0) {
            pathLog("[PathDeformer][Summary] frame=", frameCounter,
                    " changedTargets=", sChangedCount.size());
            int printed = 0;
            for (const auto& kv : sChangedCount) {
                pathLog("[PathDeformer][Summary] targetUuid=", kv.first, " hits=", kv.second);
                if (++printed >= 8) break;
            }
        }
    }
    return res;
}

void PathDeformer::runPreProcessTask(core::RenderContext& ctx) {
    bool diagStarted = beginDiagnosticFrame();
    auto currentTransform = transform();
    Mat4 currentMat = currentTransform.toMat4();
    if (!isFiniteMatrix(currentMat)) {
        recordInvalid("transformInvalid", 0, Vec2{});
        logTransformFailure("runPreProcessTask:transform", currentMat);
        disablePhysicsDriver("transformInvalid");
        matrixInvalidThisFrame = true;
        if (diagStarted) endDiagnosticFrame();
        return;
    }
    refreshInverseMatrix("runPreProcessTask:initial");
    Vec2Array origDeform = deformation;
    Deformable::runPreProcessTask(ctx);
    applyPathDeform(origDeform);
    if (invalidThisFrame && diagnosticsEnabled) {
        logDiagnostics();
    }
    if (diagStarted) endDiagnosticFrame();
}

void PathDeformer::applyPathDeform(const Vec2Array& origDeform) {
    transform();
    refreshInverseMatrix("applyPathDeform:pre");
    sanitizeOffsets(deformation);
    auto pup = puppetRef();
    const bool enableDriverStep = (pup && pup->enableDrivers);
    if (driver) {
        Vec2Array baseline = (origDeform.size() == deformation.size()) ? origDeform : deformation;
        sanitizeOffsets(baseline);
        if (!driverInitialized && enableDriverStep) {
            driver->setup();
            driverInitialized = true;
        }
        Vec2Array diffDeform = deformation;
        if (diffDeform.size() == baseline.size()) {
            for (std::size_t i = 0; i < diffDeform.size(); ++i) {
                diffDeform.xAt(i) -= baseline.xAt(i);
                diffDeform.yAt(i) -= baseline.yAt(i);
            }
        }
        sanitizeOffsets(diffDeform);

        if (vertices.size() >= 2) {
            auto prevCandidate = vertices;
            prevCandidate += diffDeform;
            prevCurve = createCurve(prevCandidate, curveType == CurveType::Bezier);
            clearCache();
            logCurveHealth("applyPathDeform:driverPrev", originalCurve, prevCurve, deformation);
            if (enableDriverStep) {
                driver->updateDefaultShape();
                Vec2 root{};
                Mat4 transformMatrix = transform().toMat4();
                if (deformation.size() > 0) {
                    auto tp = transformMatrix.transformPoint(Vec3{vertices[0].x + deformation.xAt(0), vertices[0].y + deformation.yAt(0), 0.0f});
                    root = sanitizeVec2(Vec2{tp.x, tp.y});
                }
                if (prevRootSet) {
                    Vec2 deform = sanitizeVec2(Vec2{root.x - prevRoot.x, root.y - prevRoot.y});
                    driver->reset();
                    driver->enforce(deform);
                    driver->rotate(transform().rotation.z);
                    if (physicsOnly) {
                        Vec2Array prevDeform = deformation;
                        driver->update(deformation, strength, frameCounter);
                        prevCurve = createCurve(vertices, curveType == CurveType::Bezier);
                        logCurveHealth("applyPathDeform:physicsPrev", originalCurve, prevCurve, deformation);
                        if (prevDeform.size() == deformation.size()) {
                            for (std::size_t i = 0; i < deformation.size(); ++i) {
                                deformation.set(i, Vec2{
                                    deformation.xAt(i) - prevDeform.xAt(i),
                                    deformation.yAt(i) - prevDeform.yAt(i),
                                });
                            }
                        }
                        sanitizeOffsets(deformation);
                    } else {
                        driver->update(deformation, strength, frameCounter);
                    }
                }
                prevRoot = root;
                prevRootSet = true;
            }

            auto candidate = vertices;
            candidate += deformation;
            sanitizeOffsets(candidate);
            deform(candidate);
            logCurveState("applyPathDeform");
        }
        refreshInverseMatrix("applyPathDeform:postDriver");
    } else {
        if (vertices.size() >= 2) {
            auto candidate = vertices;
            candidate += deformation;
            sanitizeOffsets(candidate);
            deform(candidate);
            logCurveState("applyPathDeform");
        }
        refreshInverseMatrix("applyPathDeform:postNoDriver");
    }
    if (diagnosticsEnabled) {
        logCurveDiag("applyPathDeform", origDeform, deformation);
        logCurveHealth("applyPathDeform", originalCurve, deformedCurve, deformation);
    }
    updateDeform();
}

void PathDeformer::deform(const Vec2Array& deformedControlPoints) {
    deformedCurve = createCurve(deformedControlPoints, curveType == CurveType::Bezier);
}

void PathDeformer::runRenderTask(core::RenderContext&) {
    // no GPU commands
}

void PathDeformer::switchDynamic(bool enablePhysics) {
    dynamicDeformation = enablePhysics;
    clearCache();
    resetDiagnostics();
    driverInitialized = false;
    prevRootSet = false;
}

void PathDeformer::notifyChange(const std::shared_ptr<Node>& target, NotifyReason reason) {
    if (reason == NotifyReason::StructureChanged && target) {
        meshCaches.erase(target->uuid);
    }
    Node::notifyChange(target, reason);
}

bool PathDeformer::setupChild(const std::shared_ptr<Node>& child) {
    Deformable::setupChild(child);
    if (!child) return true;
    std::function<void(const std::shared_ptr<Node>&)> setGroup;
    setGroup = [&](const std::shared_ptr<Node>& node) {
        setupChildNoRecurse(node);
        if (node && node->mustPropagate()) {
            for (auto& c : node->childrenRef()) setGroup(c);
        }
    };
    setGroup(child);
    return true;
}

bool PathDeformer::releaseChild(const std::shared_ptr<Node>& child) {
    std::function<void(const std::shared_ptr<Node>&)> unsetGroup;
    unsetGroup = [&](const std::shared_ptr<Node>& node) {
        releaseChildNoRecurse(node);
        if (node && node->mustPropagate()) {
            for (auto& c : node->childrenRef()) unsetGroup(c);
        }
    };
    unsetGroup(child);
    Deformable::releaseChild(child);
    return true;
}

void PathDeformer::captureTarget(const std::shared_ptr<Node>& target) {
    childrenRef().push_back(target);
    setupChildNoRecurse(target, true);
}

void PathDeformer::releaseTarget(const std::shared_ptr<Node>& target) {
    releaseChildNoRecurse(target);
    auto& ch = childrenRef();
    ch.erase(std::remove(ch.begin(), ch.end(), target), ch.end());
}

void PathDeformer::clearCache() {
    maxSegmentLength = 0.0f;
    totalLength = 0.0f;
    invalidLog.clear();
    meshCaches.clear();
}

void PathDeformer::build(bool force) {
    for (auto& c : children) setupChild(c);
    setupSelf();
    refresh();
    Node::build(force);
}

bool PathDeformer::setupChildNoRecurse(const std::shared_ptr<Node>& node, bool prepend) {
    if (!node) return true;
    auto drawable = std::dynamic_pointer_cast<Drawable>(node);
    auto grid = std::dynamic_pointer_cast<GridDeformer>(node);
    bool supportsDeform = static_cast<bool>(drawable) || static_cast<bool>(grid);

    Node::FilterHook hook;
    hook.stage = kPathFilterStage;
    hook.tag = reinterpret_cast<std::uintptr_t>(this);
    hook.func = [this](auto t, auto v, auto d, auto mat) {
        auto res = deformChildren(t, v, d, mat);
        return std::make_tuple(res.vertices, std::optional<Mat4>{}, res.changed);
    };

    auto& pre = node->preProcessFilters;
    auto& post = node->postProcessFilters;
    const auto tag = reinterpret_cast<std::uintptr_t>(this);
    pre.erase(std::remove_if(pre.begin(), pre.end(), [&](const auto& h) { return h.stage == kPathFilterStage && h.tag == tag; }), pre.end());
    post.erase(std::remove_if(post.begin(), post.end(), [&](const auto& h) { return h.stage == kPathFilterStage && h.tag == tag; }), post.end());

    if (supportsDeform) {
        cacheClosestPoints(node);
        if (dynamicDeformation) {
            if (prepend) post.insert(post.begin(), hook); else post.push_back(hook);
        } else {
            if (prepend) pre.insert(pre.begin(), hook); else pre.push_back(hook);
        }
    } else {
        meshCaches.erase(node->uuid);
    }
    return true;
}

bool PathDeformer::releaseChildNoRecurse(const std::shared_ptr<Node>& node) {
    if (!node) return true;
    meshCaches.erase(node->uuid);
    auto& pre = node->preProcessFilters;
    auto& post = node->postProcessFilters;
    const auto tag = reinterpret_cast<std::uintptr_t>(this);
    pre.erase(std::remove_if(pre.begin(), pre.end(), [&](const auto& h) { return h.stage == kPathFilterStage && h.tag == tag; }), pre.end());
    post.erase(std::remove_if(post.begin(), post.end(), [&](const auto& h) { return h.stage == kPathFilterStage && h.tag == tag; }), post.end());
    return true;
}

void PathDeformer::applyDeformToChildren(const std::vector<std::shared_ptr<core::param::Parameter>>& params, bool recursive) {
    if (driver) {
        physicsOnly = true;
        return;
    }
    if (dynamicDeformation) return;
    bool diagStarted = beginDiagnosticFrame();
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
        } else {
            if (translateChildren) {
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
        }
        if (recursive && c->mustPropagate()) {
            for (auto& gc : c->childrenRef()) apply(gc);
        }
    };
    refreshInverseMatrix("applyDeformToChildren");
    for (auto& c : children) apply(c);
    if (invalidThisFrame) ++invalidFrameCount;
    if (diagnosticsEnabled && invalidThisFrame) {
        pathLog("[PathDeformer][Diag] frame=", frameCounter, " invalidFrames=", invalidFrameCount,
                " totalInvalid=", totalInvalidCount, " lastCtx=", lastInvalidContext);
    }
    physicsOnly = true;
    rebuffer(std::vector<Vec2>{});
    if (diagStarted) endDiagnosticFrame();
}

void PathDeformer::copyFrom(const Node& src, bool clone, bool deepCopy) {
    Deformable::copyFrom(src, clone, deepCopy);
    if (auto p = dynamic_cast<const PathDeformer*>(&src)) {
        controlPoints = p->controlPoints;
        inverseMatrix = p->inverseMatrix;
        strength = p->strength;
        translateChildren = p->translateChildren;
        maxSegmentLength = p->maxSegmentLength;
        totalLength = p->totalLength;
        curveType = p->curveType;
        physicsType = p->physicsType;
        frameCounter = p->frameCounter;
    }
    rebuffer(controlPoints);
}

void PathDeformer::rebuffer(const std::vector<Vec2>& points) {
    controlPoints = points;
    vertices.resize(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        vertices.xAt(i) = points[i].x;
        vertices.yAt(i) = points[i].y;
    }
    deformation.resize(points.size());
    deformation.fill(Vec2{0.0f, 0.0f});
    originalCurve = createCurve(vertices, curveType == CurveType::Bezier);
    deformedCurve = createCurve(vertices, curveType == CurveType::Bezier);
    prevCurve = originalCurve ? createCurve(vertices, curveType == CurveType::Bezier) : nullptr;
    checkBaselineDegeneracy(points);
    rebuildCurveSamples();
    clearCache();
    driverInitialized = false;
    prevRootSet = false;
    ensureDiagnosticCapacity(deformation.size());
}

void PathDeformer::serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const {
    Deformable::serializeSelfImpl(serializer, recursive, flags);
    if (has_flag(flags, SerializeNodeFlags::State)) {
        serializer.putKey("physics_only");
        serializer.putValue(physicsOnly);
        serializer.putKey("dynamic_deformation");
        serializer.putValue(dynamicDeformation);
        serializer.putKey("curve_type");
        serializer.putValue(static_cast<int>(curveType));
        serializer.putKey("strength");
        serializer.putValue(strength);
        serializer.putKey("translateChildren");
        serializer.putValue(translateChildren);
        serializer.putKey("curveType");
        serializer.putValue(static_cast<int>(curveType));
        serializer.putKey("physicsType");
        serializer.putValue(static_cast<int>(physicsType));
    }
    if (has_flag(flags, SerializeNodeFlags::Geometry)) {
        serializer.putKey("controlPoints");
        auto arr = serializer.listBegin();
        for (const auto& p : controlPoints) {
            serializer.elemBegin();
            serializer.putKey("x"); serializer.putValue(p.x);
            serializer.putKey("y"); serializer.putValue(p.y);
        }
        serializer.listEnd(arr);
    }
    if (has_flag(flags, SerializeNodeFlags::Links) && driver) {
        boost::property_tree::ptree phys;
        std::string typeStr = (physicsType == PhysicsType::SpringPendulum) ? "SpringPendulum" : "Pendulum";
        phys.put("type", typeStr);
        if (auto adapter = dynamic_cast<ConnectedDriverAdapter*>(driver.get())) {
            PhysicsDriverState st;
            adapter->getState(st);
            boost::property_tree::ptree angles;
            for (auto v : st.angles) angles.push_back({"", boost::property_tree::ptree(std::to_string(v))});
            boost::property_tree::ptree angVel;
            for (auto v : st.angularVelocities) angVel.push_back({"", boost::property_tree::ptree(std::to_string(v))});
            boost::property_tree::ptree lens;
            for (auto v : st.lengths) lens.push_back({"", boost::property_tree::ptree(std::to_string(v))});
            phys.add_child("angles", angles);
            phys.add_child("angularVelocities", angVel);
            phys.add_child("lengths", lens);
            phys.put("base.x", st.base.x);
            phys.put("base.y", st.base.y);
            phys.put("externalForce.x", st.externalForce.x);
            phys.put("externalForce.y", st.externalForce.y);
            phys.put("damping", st.damping);
            phys.put("restore", st.restore);
            phys.put("timeStep", st.timeStep);
            phys.put("gravity", st.gravity);
            phys.put("inputScale", st.inputScale);
            phys.put("worldAngle", st.worldAngle);
            phys.put("propagateScale", st.propagateScale);
            phys.put("gravityVec.x", st.gravityVec.x);
            phys.put("gravityVec.y", st.gravityVec.y);
            phys.put("springConstant", st.springConstant);
            phys.put("restorationConstant", st.restorationConstant);
        }
        serializer.root.add_child("physics", phys);
    }
}

::nicxlive::core::serde::SerdeException PathDeformer::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    try {
        if (auto err = Deformable::deserializeFromFghj(data)) {
            return err;
        }
        driver.reset();

        auto parseCurveType = [](const ::nicxlive::core::serde::Fghj& node, CurveType& out) {
            try {
                out = static_cast<CurveType>(node.get_value<int>());
                return;
            } catch (...) {
            }
            try {
                auto s = node.get_value<std::string>();
                if (s == "Bezier") out = CurveType::Bezier;
                else if (s == "Spline") out = CurveType::Spline;
            } catch (...) {
            }
        };
        auto parsePhysicsType = [](const ::nicxlive::core::serde::Fghj& node, PhysicsType& out) {
            try {
                out = static_cast<PhysicsType>(node.get_value<int>());
                return;
            } catch (...) {
            }
            try {
                auto s = node.get_value<std::string>();
                if (s == "SpringPendulum") out = PhysicsType::SpringPendulum;
                else if (s == "Pendulum") out = PhysicsType::Pendulum;
            } catch (...) {
            }
        };
    if (auto st = data.get_child_optional("strength")) {
        strength = st->get_value<float>();
    }
    if (auto tc = data.get_child_optional("translateChildren")) {
        translateChildren = tc->get_value<bool>();
    }
        if (auto ct = data.get_child_optional("curveType")) {
            parseCurveType(*ct, curveType);
        }
        if (auto ct2 = data.get_child_optional("curve_type")) {
            parseCurveType(*ct2, curveType);
        }
        if (auto pt = data.get_child_optional("physicsType")) {
            parsePhysicsType(*pt, physicsType);
        }
    if (auto po = data.get_child_optional("physics_only")) {
        physicsOnly = po->get_value<bool>();
    }
    if (auto dd = data.get_child_optional("dynamic_deformation")) {
        dynamicDeformation = dd->get_value<bool>();
    } else {
        dynamicDeformation = false;
    }
    if (auto verts = data.get_child_optional("vertices")) {
        controlPoints.clear();
        for (auto it = verts->begin(); it != verts->end();) {
            Vec2 p{};
            p.x = it->second.get_value<float>();
            ++it;
            if (it == verts->end()) break;
            p.y = it->second.get_value<float>();
            ++it;
            controlPoints.push_back(p);
        }
    } else if (auto cps = data.get_child_optional("controlPoints")) {
        // Backward-compat fallback (non-D format)
        controlPoints.clear();
        for (const auto& cp : *cps) {
            Vec2 p{};
            p.x = cp.second.get<float>("x", 0.0f);
            p.y = cp.second.get<float>("y", 0.0f);
            controlPoints.push_back(p);
        }
    }
    rebuffer(controlPoints);
    if (auto phy = data.get_child_optional("physics")) {
        std::string type = phy->get<std::string>("type", "Pendulum");
        if (type == "SpringPendulum") physicsType = PhysicsType::SpringPendulum;
        else physicsType = PhysicsType::Pendulum;
        driver = createPhysicsDriver();
        if (auto adapter = dynamic_cast<ConnectedDriverAdapter*>(driver.get())) {
            PhysicsDriverState st;
            st.type = type;
            if (auto ang = phy->get_child_optional("angles")) {
                for (const auto& v : *ang) st.angles.push_back(v.second.get_value<float>());
            }
            if (auto av = phy->get_child_optional("angularVelocities")) {
                for (const auto& v : *av) st.angularVelocities.push_back(v.second.get_value<float>());
            }
            if (auto lens = phy->get_child_optional("lengths")) {
                for (const auto& v : *lens) st.lengths.push_back(v.second.get_value<float>());
            }
            st.base.x = phy->get<float>("base.x", 0.0f);
            st.base.y = phy->get<float>("base.y", 0.0f);
            st.externalForce.x = phy->get<float>("externalForce.x", 0.0f);
            st.externalForce.y = phy->get<float>("externalForce.y", 0.0f);
            st.damping = phy->get<float>("damping", st.damping);
            st.restore = phy->get<float>("restore_constant", phy->get<float>("restore", st.restore));
            st.timeStep = phy->get<float>("timeStep", st.timeStep);
            st.gravity = phy->get<float>("gravity", st.gravity);
            st.inputScale = phy->get<float>("input_scale", phy->get<float>("inputScale", st.inputScale));
            st.worldAngle = phy->get<float>("worldAngle", st.worldAngle);
            st.propagateScale = phy->get<float>("propagate_scale", phy->get<float>("propagateScale", st.propagateScale));
            st.gravityVec.x = phy->get<float>("gravityVec.x", st.gravityVec.x);
            st.gravityVec.y = phy->get<float>("gravityVec.y", st.gravityVec.y);
            st.springConstant = phy->get<float>("springConstant", st.springConstant);
            st.restorationConstant = phy->get<float>("restorationConstant", st.restorationConstant);
            adapter->setState(st);
        }
    }
        return std::nullopt;
    } catch (const std::exception& e) {
        return std::string(e.what());
    }
}

// simple pendulum-ish drivers
void PathDeformer::PendulumDriver::update(Vec2Array& offsets, float strength, uint64_t frame) {
    float phase = static_cast<float>(frame) * 0.05f;
    for (std::size_t i = 0; i < offsets.size(); ++i) {
        offsets.xAt(i) = std::sin(phase + i * 0.1f) * strength * 0.1f;
        offsets.yAt(i) = std::cos(phase + i * 0.1f) * strength * 0.1f;
    }
}

void PathDeformer::SpringPendulumDriver::update(Vec2Array& offsets, float strength, uint64_t frame) {
    float phase = static_cast<float>(frame) * 0.08f;
    velocity = velocity * 0.9f + std::sin(phase) * 0.05f;
    for (std::size_t i = 0; i < offsets.size(); ++i) {
        offsets.xAt(i) = velocity * strength * 0.2f;
        offsets.yAt(i) = velocity * strength * 0.2f;
    }
}

} // namespace nicxlive::core::nodes
