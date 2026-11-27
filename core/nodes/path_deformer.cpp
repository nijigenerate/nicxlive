#include "path_deformer.hpp"
#include "utils.hpp"

#include "../common/utils.hpp"
#include "../common/veca_axes.hpp"
#include "../compat/scope_exit.hpp"
#include "../compat/phobos_decimal_format.hpp"
#include "../render.hpp"
#include "../puppet.hpp"
#include "../serde.hpp"
#include "../param/parameter.hpp"
#include "../debug_log.hpp"
#include "../render/profiler.hpp"
#include "curve.hpp"
#include "deformer/drivers/phys.hpp"
#include "drawable.hpp"
#include "grid_deformer.hpp"
#include "projectable.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <optional>
#include <sstream>
#include <unordered_set>
#include <unordered_map>

namespace nicxlive::core::nodes {

using nicxlive::core::common::transformAssign;
using nicxlive::core::common::transformAdd;
using nicxlive::core::common::gatherVec2;
using nicxlive::core::common::scatterAddVec2;

namespace {
Node::FilterHook sourcePathFilter(std::shared_ptr<PathDeformer> receiver) {
    Node::FilterHook result;
    result.stage = 1;
    result.tag = reinterpret_cast<std::uintptr_t>(receiver.get());
    result.func = Node::FilterHook::Func::fromMethod<&SourceDeformation::deformChildrenSource>(receiver);
    return result;
}

Node::FilterArray sourcePathUpsertFilter(Node::FilterArray filters,
    std::shared_ptr<PathDeformer> receiver, bool prepend) {
    const auto filter = sourcePathFilter(receiver);
    if (prepend) return ngUpsertSource<Node::FilterHook, true>(filters, filter);
    return ngUpsertSource(filters, filter);
}

Node::FilterArray sourcePathRemoveFilter(Node::FilterArray filters, std::shared_ptr<PathDeformer> receiver) {
    return ngRemoveByValueSource(filters, sourcePathFilter(receiver));
}

std::string sourcePathFixed(float value, std::size_t precision) {
    d_runtime::decimal_format::DecimalFormatSpec spec;
    spec.conversion = 'f';
    spec.precision = precision;
    return d_runtime::decimal_format::ngFormatDecimal(value, spec);
}

std::string sourcePathFormatVec2(Vec2 value) {
    std::string result = "(";
    result += sourcePathFixed(value.x, 4);
    result += ", ";
    result += sourcePathFixed(value.y, 4);
    result += ")";
    return result;
}

std::string sourcePathSummarizePoints(const Vec2Array& points, std::size_t maximum = 4) {
    std::string result = "len=";
    char count[3 * sizeof(std::size_t) + 1];
    const auto converted = std::to_chars(count, count + sizeof(count), points.size());
    result.append(count, converted.ptr);
    result += " [";
    const auto length = std::min(points.size(), maximum);
    for (std::size_t i = 0; i < length; ++i) {
        const Vec2 point{points.xAt(i), points.yAt(i)};
        result += sourcePathFormatVec2(point);
        if (i + 1 < length) result += ", ";
    }
    if (points.size() > maximum) result += ", ...";
    result += "]";
    return result;
}

std::string sourcePathMatrixSummary(Mat4 matrix) {
    std::string result = "[";
    for (int row = 0; row < 4; ++row) {
        result += "[";
        for (int column = 0; column < 4; ++column) {
            result += sourcePathFixed(matrix[row][column], 3);
            if (column < 3) result += ", ";
        }
        result += "]";
        if (row < 3) result += ", ";
    }
    result += "]";
    return result;
}

template<class Values> decltype(auto) sourcePathAt(Values& values, std::size_t index, std::size_t line) {
    if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
        index, values.size(), d_runtime::RetainedString::fromStaticLiteral(
            "source/nijilive/core/nodes/deformer/path.d"), line);
    return values[index];
}

Vec2 sourcePathLoggedPoint(const Vec2Array& values, std::size_t index) {
#ifndef NDEBUG
    if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::AssertError>(
        "veca index out of range", "source/nijilive/math/veca.d", 139);
#endif
    if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
        index, values.size(), d_runtime::RetainedString::fromStaticLiteral(
            "source/nijilive/math/veca.d"), 467);
    return {values.xAt(index), values.yAt(index)};
}

void sourcePathStoreLoggedPoint(Vec2Array& values, std::size_t index, Vec2 point) {
#ifndef NDEBUG
    if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::AssertError>(
        "veca index out of range", "source/nijilive/math/veca.d", 139);
#endif
    if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
        index, values.size(), d_runtime::RetainedString::fromStaticLiteral(
            "source/nijilive/math/veca.d"), 475);
    values.xAt(index) = point.x;
    values.yAt(index) = point.y;
}

template<bool Add> void sourcePathArithmetic(Vec2Array& destination, const Vec2Array source) {
#ifndef NDEBUG
    if (destination.size() != source.size()) throw d_runtime::ngStaticError<d_runtime::AssertError>(
        "Mismatched vector lengths", "source/nijilive/math/veca.d", 181);
#endif
    const auto length = destination.size();
    if (!length) return;
    const auto axis = [&](bool y) {
        const auto read = [y](const Vec2Array& values, std::size_t i) {
            return y ? values.yAt(i) : values.xAt(i);
        };
        const auto cell = [y](Vec2Array& values, std::size_t i) -> float& {
            return y ? values.yAt(i) : values.xAt(i);
        };
        bool simd = false;
        if (length == source.size()) {
            const auto destinationAddress = reinterpret_cast<std::uintptr_t>(&cell(destination, 0));
            const auto sourceAddress = reinterpret_cast<std::uintptr_t>(
                y ? source.yPtr_ + source.laneBase_ : source.xPtr_ + source.laneBase_);
            simd = ((destinationAddress | sourceAddress) & 15) == 0;
        }
        std::size_t i = 0;
        if (simd) {
            for (; i + 4 <= length; i += 4) {
                float left[4], right[4];
                for (std::size_t lane = 0; lane < 4; ++lane) left[lane] = read(destination, i + lane);
                for (std::size_t lane = 0; lane < 4; ++lane) right[lane] = read(source, i + lane);
                for (std::size_t lane = 0; lane < 4; ++lane) {
                    if constexpr (Add) left[lane] += right[lane];
                    else left[lane] -= right[lane];
                }
                for (std::size_t lane = 0; lane < 4; ++lane) cell(destination, i + lane) = left[lane];
            }
        }
        // LDC's unaligned/mismatched arrayOp fallback is scalar (operations.d221).
        for (; i < length; ++i) {
            if (i >= source.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
                i, source.size(), d_runtime::RetainedString::fromStaticLiteral(
                    "core/internal/array/operations.d"), 73);
            if constexpr (Add) cell(destination, i) += read(source, i);
            else cell(destination, i) -= read(source, i);
        }
    };
    axis(false);
    axis(true);
}

void sourcePathSubtract(Vec2Array& destination, const Vec2Array source) {
    sourcePathArithmetic<false>(destination, source);
}

void sourcePathAdd(Vec2Array& destination, const Vec2Array source) {
    sourcePathArithmetic<true>(destination, source);
}

float sourcePathCurveScale(const Vec2Array& points) {
    float scale = 0;
    const auto count = points.size();
    for (std::size_t i = 0; i < count; ++i) {
        const float x = points.xAt(i), y = points.yAt(i);
        if (std::isnan(x) || std::isnan(y)) continue;
        const float magnitude = std::sqrt(x * x + y * y);
        if (magnitude > scale) scale = magnitude;
    }
    return scale;
}

Vec4 sourcePathMatrixVector(Mat4 matrix, Vec4 vector) {
    const float components[4]{vector.x, vector.y, vector.z, vector.w};
    float result[4]{};
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            result[row] = math::detail::inmathAccumulate(result[row],
                math::detail::inmathProduct(matrix[row][column], components[column]));
    return {result[0], result[1], result[2], result[3]};
}

Vec2 sourcePathFirst(const Vec2Array& values) {
#ifndef NDEBUG
    if (values.empty()) throw d_runtime::ngStaticError<d_runtime::AssertError>(
        "veca index out of range", "source/nijilive/math/veca.d", 139);
#endif
    if (values.empty()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
        0, 0, d_runtime::RetainedString::fromStaticLiteral("source/nijilive/math/veca.d"), 467);
    return {values.xAt(0), values.yAt(0)};
}

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
    std::ostringstream oss;
    (oss << ... << args);
    oss << '\n';
    NJCX_DBG_LOG("%s", oss.str().c_str());
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

PathDeformer::PathDeformer(CurveType type) {
    curveType = type;
    requirePreProcessTask();
    diagnosticsEnabled = false;
    originalCurve = nicxlive::core::nodes::createCurve(Vec2Array{}, curveType == CurveType::Bezier);
    deformedCurve = nicxlive::core::nodes::createCurve(Vec2Array{}, curveType == CurveType::Bezier);
    prevCurve.reset();
    std::destroy_at(&vertices);
    std::construct_at(&vertices, originalCurve->controlPoints());
    driver.reset();
    prevRootSet = false;
}

std::shared_ptr<PathDeformer> PathDeformer::create(std::shared_ptr<Node> parent, CurveType type) {
    ngRegisterModelResourceClassNames();
    auto result = std::shared_ptr<PathDeformer>(new PathDeformer(DeferredConstruction{}));
    result->constructPathOwned(parent, type);
    return result;
}

std::shared_ptr<PathDeformer> PathDeformer::allocateSourcePathReceiver() {
    ngRegisterModelResourceClassNames();
    return std::allocate_shared<PathDeformer>(
        d_runtime::SourceAllocator<PathDeformer>{}, DeferredConstruction{});
}

void PathDeformer::constructPathOwned(std::shared_ptr<Node> parent, CurveType type) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    constructDeformableOwned(parent);
    requirePreProcessTask();
    curveType = type;
    originalCurve = createCurve(Vec2Array{});
    deformedCurve = createCurve(Vec2Array{});
    setDriverSource({});
    prevRootSet = false;
}

void PathDeformer::ConnectedDriverAdapter::setup() {
    d_runtime::ngRequireObject(impl_.get())->setupSource();
}

void PathDeformer::ConnectedDriverAdapter::update(Vec2Array& offsets, float strength, uint64_t frame) {
    (void)offsets; (void)strength; (void)frame;
    d_runtime::ngRequireObject(impl_.get())->updateSource();
}

void PathDeformer::ConnectedDriverAdapter::updateDefaultShape() {
    d_runtime::ngRequireObject(impl_.get())->updateDefaultShapeSource();
}

void PathDeformer::ConnectedDriverAdapter::retarget(std::shared_ptr<PathDeformer> deformer) {
    d_runtime::ngRequireObject(impl_.get())->retargetSource(deformer);
}

void PathDeformer::ConnectedDriverAdapter::serializeSource(core::serde::TypedJsonWriter& writer) const {
    const auto state = writer.structBegin();
    d_runtime::ngRequireObject(impl_.get())->serializeSource(writer);
    writer.structEnd(state);
}

core::serde::ReadError PathDeformer::ConnectedDriverAdapter::deserializeSource(core::serde::TypedJsonView data) {
    return d_runtime::ngRequireObject(impl_.get())->deserializeSource(data);
}

void PathDeformer::ConnectedDriverAdapter::copyFromSource(std::shared_ptr<PhysicsDriver> source) {
    const auto adapter = std::dynamic_pointer_cast<ConnectedDriverAdapter>(source);
    d_runtime::ngRequireObject(impl_.get())->copyFromSource(adapter ? adapter->impl() : nullptr);
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

std::shared_ptr<PathDeformer::PhysicsDriver> PathDeformer::createPhysicsSource() {
    const auto lifetime = weak_from_this().lock();
    if (hasDegenerateBaseline) {
        (void)getName();
        return {};
    }
    const auto receiver = d_runtime::ngRequireObject(std::dynamic_pointer_cast<PathDeformer>(lifetime));
    if (physicsType == PhysicsType::SpringPendulum)
        return std::make_shared<ConnectedDriverAdapter>(
            std::make_unique<ConnectedSpringPendulumDriver>(receiver), receiver.get());
    return std::make_shared<ConnectedDriverAdapter>(
        std::make_unique<ConnectedPendulumDriver>(receiver), receiver.get());
}

void PathDeformer::setDriverSource(std::shared_ptr<PhysicsDriver> value) {
    const auto lifetime = weak_from_this().lock();
    const auto retarget = [&] {
        if (driver) {
            if (hasDegenerateBaseline) sourceDisablePhysicsDriver("degenerateBaseline");
            else {
                const auto current = driver;
                const auto self = d_runtime::ngRequireObject(std::dynamic_pointer_cast<PathDeformer>(lifetime));
                current->retarget(self);
            }
        }
    };
    if (common::ngClassObjectIs(driver, value)) {
        retarget();
        return;
    }
    driver = value;
    driverInitialized = false;
    prevRootSet = false;
    retarget();
    clearCache();
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

Vec2Array PathDeformer::getVerticesSource(std::shared_ptr<Node> node) {
    if (const auto deformable = std::dynamic_pointer_cast<Deformable>(node))
        return deformable->getVertices();
    const auto translation = d_runtime::ngRequireObject(node)->transform().translation;
    Vec2Array result;
    result.resize(1);
    result.xAt(0) = translation.x;
    result.yAt(0) = translation.y;
    return result;
}

void PathDeformer::cacheClosestPointsSource(std::shared_ptr<Node> node, int nSamples) {
    const auto retainedReceiver = weak_from_this().lock();
    // path.d96-117: retain the actual argument, then obtain each authored
    // vertices descriptor anew. A getter or key callback can change state.
    const auto target = node;
    const auto vertices = getVerticesSource(target);
    meshCaches[target] = common::SharedArray<float>(vertices.size(),
        std::numeric_limits<float>::quiet_NaN());
    if (prevCurve) {
        const auto forward = d_runtime::ngRequireObject(target)->transform().toMat4();
        const auto inverse = Mat4::inverse(transform().toMat4());
        const auto matrix = Mat4::multiply(inverse, forward);
        const auto currentVertices = getVerticesSource(target);
        for (std::size_t i = 0; i < currentVertices.size(); ++i) {
            const float vertex[4]{currentVertices.xAt(i), currentVertices.yAt(i), 0, 1};
            float projected[4]{};
            for (std::size_t column = 0; column < 4; ++column) {
                for (std::size_t row = 0; row < 4; ++row)
                    projected[row] = math::detail::inmathAccumulate(projected[row],
                        math::detail::inmathProduct(matrix[static_cast<int>(row)][column], vertex[column]));
            }
            // D assignment evaluates the closest-point RHS before its AA/index
            // lvalue. Do not hold an entry/curve reference across that call.
            const auto curve = prevCurve;
            const auto sourceCurve = std::dynamic_pointer_cast<SourceCurveEvaluation>(curve);
            const float parameter = d_runtime::ngRequireObject(sourceCurve)->closestPointSource(
                Vec2{projected[0], projected[1]}, nSamples);
            auto cell = meshCaches[target];
            auto& samples = static_cast<common::SharedArray<float>&>(cell);
            if (i >= samples.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
                i, samples.size(), d_runtime::RetainedString::fromStaticLiteral(
                    "source/nijilive/core/nodes/deformer/path.d"), 114);
            samples[i] = parameter;
        }
    }
}

void PathDeformer::cacheClosestPoints(const std::shared_ptr<Node>& node, int nSamples) {
    // Native compatibility can accept an older Curve implementor. Actual Source
    // curves take the real virtual Source body, including build/capture preloads.
    if (!prevCurve || std::dynamic_pointer_cast<SourceCurveEvaluation>(prevCurve)) {
        cacheClosestPointsSource(node, nSamples);
        return;
    }
    const auto retainedReceiver = weak_from_this().lock();
    const auto target = node;
    const auto verticesFor = [](const std::shared_ptr<Node>& value) -> Vec2Array {
        if (const auto deformable = std::dynamic_pointer_cast<Deformable>(value))
            return deformable->getVertices();
        const auto translation = d_runtime::ngRequireObject(value)->transform().translation;
        Vec2Array result;
        result.resize(1);
        result.xAt(0) = translation.x;
        result.yAt(0) = translation.y;
        return result;
    };
    const auto vertices = verticesFor(target);
    meshCaches[target] = common::SharedArray<float>(vertices.size(), std::numeric_limits<float>::quiet_NaN());
    if (!prevCurve) return;
    const auto forward = d_runtime::ngRequireObject(target)->transform().toMat4();
    const auto inverse = Mat4::inverse(transform().toMat4());
    const auto matrix = Mat4::multiply(inverse, forward);
    const auto currentVertices = verticesFor(target);
    for (std::size_t i = 0; i < currentVertices.size(); ++i) {
        const auto transformed = matrix.transformPoint(Vec3{currentVertices.xAt(i), currentVertices.yAt(i), 0});
        const auto curve = prevCurve;
        const auto parameter = d_runtime::ngRequireObject(curve)->closestPoint(
            Vec2{transformed.x, transformed.y}, nSamples);
        auto cell = meshCaches[target];
        auto& samples = static_cast<common::SharedArray<float>&>(cell);
        if (i >= samples.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
            i, samples.size(), d_runtime::RetainedString::fromStaticLiteral(
                "source/nijilive/core/nodes/deformer/path.d"), 114);
        samples[i] = parameter;
    }
}

void PathDeformer::recordInvalid(const std::string& ctx, std::size_t idx, const Vec2& value) {
    invalidThisFrame = true;
    diagnostics.invalidThisFrame = true;
    if (!diagnosticsEnabled && !preserveInvalidLog) return;
    ensureDiagnosticCapacity(idx + 1);
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
    diagnostics.invalidLastLoggedContext[idx] = std::string(invalidLastLoggedContext[idx].view());
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

void PathDeformer::logCurveHealth(const std::string& ctx, const std::shared_ptr<Curve>& a, const std::shared_ptr<Curve>& b, const Vec2Array& def) {
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
    if (!diagnosticsEnabled && !preserveInvalidLog) {
        invalidThisFrame = false;
        matrixInvalidThisFrame = false;
        diagnostics.invalidThisFrame = false;
        return true;
    }
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
    if (!diagnosticsEnabled && !preserveInvalidLog) {
        diagnosticsFrameActive = false;
        diagnostics.invalidFrameCount = invalidFrameCount;
        diagnostics.totalInvalidCount = totalInvalidCount;
        diagnostics.invalidThisFrame = invalidThisFrame;
        diagnostics.diagnosticsFrameActive = false;
        return;
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
            if (i < invalidLastLoggedContext.size())
                invalidLastLoggedContext[i] = common::NullableString::fromStaticLiteral("");
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
    Mat4 global = requireFiniteMatrix(transform().toMat4(), ctx + ":global", okGlobal);
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
    if (invalidPerIndex.size() < n) invalidPerIndex.resize(n);
    if (invalidConsecutive.size() < n) invalidConsecutive.resize(n);
    if (invalidLastFrame.size() < n) invalidLastFrame.resize(n, 0);
    if (invalidLastValue.size() < n) invalidLastValue.resize(n, Vec2{});
    if (invalidIndexThisFrame.size() < n) invalidIndexThisFrame.resize(n);
    if (invalidStreakStartFrame.size() < n) invalidStreakStartFrame.resize(n);
    if (invalidLastLoggedFrame.size() < n) invalidLastLoggedFrame.resize(n);
    if (invalidLastLoggedCount.size() < n) invalidLastLoggedCount.resize(n);
    if (invalidLastLoggedValueWasNaN.size() < n) invalidLastLoggedValueWasNaN.resize(n);
    if (curveDiagTargetScale.size() < n) curveDiagTargetScale.resize(n, 0.0f);
    if (curveDiagReferenceScale.size() < n) curveDiagReferenceScale.resize(n, 0.0f);
    if (curveDiagHasNaN.size() < n) curveDiagHasNaN.resize(n, false);
    if (curveDiagCollapsed.size() < n) curveDiagCollapsed.resize(n, false);
    if (invalidLastLoggedValue.size() < n) {
        const auto old = invalidLastLoggedValue.size();
        invalidLastLoggedValue.resize(n);
        for (auto i = old; i < n; ++i) {
            invalidLastLoggedValue.xAt(i) = 0;
            invalidLastLoggedValue.yAt(i) = 0;
        }
    }
    if (invalidLastLoggedContext.size() < n) invalidLastLoggedContext.resize(n);
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
        const float dx = pts[i].x - pts[i - 1].x;
        const float dy = pts[i].y - pts[i - 1].y;
        const float segmentLength = std::sqrt(dx * dx + dy * dy);
        if (!std::isfinite(segmentLength) || segmentLength <= 1e-6f) {
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

void PathDeformer::deformSource(Vec2Array points) {
    deformedCurve = createCurve(points);
}

void PathDeformer::sourceSanitizeOffsets(Vec2Array& values) {
    const auto length = values.size();
    for (std::size_t i = 0; i < length; ++i) {
        if (!std::isfinite(values.xAt(i)) || !std::isfinite(values.yAt(i))) {
            values.xAt(i) = 0;
            values.yAt(i) = 0;
        }
    }
}

void PathDeformer::sourceRefreshInverseMatrix(const std::string& context) {
    const auto matrix = sourceRequireFiniteMatrix(globalTransform.toMat4(), context + ":global");
    inverseMatrix = sourceSafeInverse(matrix, context + ":inverse");
}

void PathDeformer::sourceLogTransformFailure(const std::string& context, const Transform& value) {
    if (loggedTransformInvalid) return;
    loggedTransformInvalid = true;
    const auto translation = sourcePathFormatVec2({value.translation.x, value.translation.y});
    const auto scale = sourcePathFormatVec2(value.scale);
    (void)translation;
    (void)scale;
    (void)getName();
    (void)context;
    (void)value.rotation.z;
    (void)sourcePathMatrixSummary(value.toMat4());
}

void PathDeformer::sourceHandleInvalidDeformation(const std::string& context, const Vec2Array fallback) {
    (void)getName();
    (void)context;
    invalidThisFrame = true;
    if (!fallback.empty() && fallback.size() == deformation.size()) deformation = fallback.dup();
}

bool PathDeformer::sourceLogCurveHealth(std::string context, std::shared_ptr<Curve> reference,
    std::shared_ptr<Curve> target, const Vec2Array& snapshot) {
    if (!target) return false;
    const auto targetPoints = target->controlPoints();
    bool hasNaN = false;
    const auto pointCount = targetPoints.size();
    for (std::size_t i = 0; i < pointCount; ++i) {
        if (std::isnan(targetPoints.xAt(i)) || std::isnan(targetPoints.yAt(i))) {
            hasNaN = true;
            break;
        }
    }
    const float targetScale = sourcePathCurveScale(targetPoints);
    float referenceScale = 0;
    Vec2Array referencePoints;
    if (reference) {
        referencePoints = reference->controlPoints();
        referenceScale = sourcePathCurveScale(referencePoints);
    }
    bool collapsed = false;
    if (reference && referenceScale > 1e-6f) collapsed = targetScale < referenceScale * 1e-3f;
    bool emitted = false;
    const bool invalid = hasNaN || collapsed;
    const common::NullableString key(context);
    const auto state = curveDiagLogStates.findRetained(key);
    CurveDiagLogState previous;
    bool hadPrevious = false;
    if (state) {
        previous = *state;
        hadPrevious = true;
        if ((previous.hasNaN || previous.collapsed) && !invalid) {
            const auto start = previous.firstInvalidFrame ? previous.firstInvalidFrame : previous.lastFrame;
            const auto frames = previous.lastFrame >= start ? previous.lastFrame - start + 1 : 1;
            (void)frames;
            (void)getName();
            emitted = true;
        }
    }
    if (invalid) {
        bool logNow = true;
        if (hadPrevious) {
            const bool same = previous.hasNaN == hasNaN && previous.collapsed == collapsed &&
                approxEqual(previous.targetScale, targetScale) && approxEqual(previous.referenceScale, referenceScale);
            if (same && frameCounter < previous.lastFrame + std::size_t{15}) logNow = false;
        }
        if (logNow) {
            auto start = frameCounter;
            if (hadPrevious && (previous.hasNaN || previous.collapsed))
                start = previous.firstInvalidFrame ? previous.firstInvalidFrame : previous.lastFrame;
            const auto frames = frameCounter >= start ? frameCounter - start + 1 : 1;
            (void)frames;
            (void)getName();
            (void)sourcePathSummarizePoints(referencePoints);
            (void)sourcePathSummarizePoints(targetPoints);
            (void)sourcePathSummarizePoints(snapshot);
            emitted = true;
        }
    }
    std::size_t firstInvalid = 0;
    if (invalid) {
        firstInvalid = hadPrevious && (previous.hasNaN || previous.collapsed) ?
            (previous.firstInvalidFrame ? previous.firstInvalidFrame : previous.lastFrame) : frameCounter;
    }
    curveDiagLogStates[key] = CurveDiagLogState{hasNaN, collapsed, targetScale, referenceScale,
        static_cast<std::size_t>(frameCounter), firstInvalid};
    return emitted;
}

void PathDeformer::sourceLogCurveState(const std::string& context) {
    const auto snapshot = deformation;
    bool logged = false;
    if (originalCurve && prevCurve)
        logged = sourceLogCurveHealth(context + ":prev", originalCurve, prevCurve, snapshot) || logged;
    else if (prevCurve)
        logged = sourceLogCurveHealth(context + ":prev", {}, prevCurve, snapshot) || logged;
    if (originalCurve && deformedCurve)
        logged = sourceLogCurveHealth(context + ":deformed", originalCurve, deformedCurve, snapshot) || logged;
    else if (deformedCurve)
        logged = sourceLogCurveHealth(context + ":deformed", {}, deformedCurve, snapshot) || logged;
    if (!logged && prevCurve && deformedCurve)
        (void)sourceLogCurveHealth(context + ":diff", prevCurve, deformedCurve, snapshot);
}

void PathDeformer::sourceCheckBaselineDegeneracy(const Vec2Array& points) {
    hasDegenerateBaseline = false;
    degenerateSegmentIndices.resize(0);
    if (points.size() < 2) return;
    std::string segments;
    const auto count = points.size();
    for (std::size_t i = 1; i < count; ++i) {
        const float x = points.xAt(i) - points.xAt(i - 1);
        const float y = points.yAt(i) - points.yAt(i - 1);
        const float length = std::sqrt(x * x + y * y);
        if (!std::isfinite(length) || length <= 1e-6f) {
            segments += hasDegenerateBaseline ? ", " : "[";
            char text[3 * sizeof(std::size_t) + 1];
            auto end = std::to_chars(text, text + sizeof(text), i - 1).ptr;
            segments.append(text, end);
            segments += "->";
            end = std::to_chars(text, text + sizeof(text), i).ptr;
            segments.append(text, end);
            degenerateSegmentIndices.push_back(i - 1);
            hasDegenerateBaseline = true;
        }
    }
    if (hasDegenerateBaseline) {
        segments += "]";
        (void)getName();
        (void)sourcePathSummarizePoints(points);
        sourceDisablePhysicsDriver("degenerateBaseline");
    }
}

void PathDeformer::sourceEnsureDiagnosticCapacity(std::size_t length) {
    if (invalidIndexThisFrame.size() == length) return;
    invalidIndexThisFrame.resize(length);
    invalidConsecutive.resize(length);
    invalidPerIndex.resize(length);
    invalidStreakStartFrame.resize(length);
    invalidLastLoggedFrame.resize(length);
    invalidLastLoggedCount.resize(length);
    invalidLastLoggedValueWasNaN.resize(length);
    invalidLastLoggedValue.resize(length);
    invalidLastLoggedContext.resize(length);
    std::fill(invalidIndexThisFrame.begin(), invalidIndexThisFrame.end(), false);
    std::fill(invalidConsecutive.begin(), invalidConsecutive.end(), 0);
    std::fill(invalidPerIndex.begin(), invalidPerIndex.end(), 0);
    std::fill(invalidStreakStartFrame.begin(), invalidStreakStartFrame.end(), 0);
    std::fill(invalidLastLoggedFrame.begin(), invalidLastLoggedFrame.end(), 0);
    std::fill(invalidLastLoggedCount.begin(), invalidLastLoggedCount.end(), 0);
    std::fill(invalidLastLoggedValueWasNaN.begin(), invalidLastLoggedValueWasNaN.end(), false);
    for (std::size_t i = 0; i < length; ++i) invalidLastLoggedValue.xAt(i) = 0;
    for (std::size_t i = 0; i < length; ++i) invalidLastLoggedValue.yAt(i) = 0;
    std::fill(invalidLastLoggedContext.begin(), invalidLastLoggedContext.end(),
        common::NullableString::fromStaticLiteral(""));
}

bool PathDeformer::sourceBeginDiagnosticFrame() {
    if (diagnosticsFrameActive) return false;
    diagnosticsFrameActive = true;
    ++frameCounter;
    sourceEnsureDiagnosticCapacity(deformation.size());
    std::fill(invalidIndexThisFrame.begin(), invalidIndexThisFrame.end(), false);
    invalidThisFrame = false;
    matrixInvalidThisFrame = false;
    return true;
}

void PathDeformer::sourceEndDiagnosticFrame() {
    if (!diagnosticsFrameActive) return;
    if (invalidThisFrame || matrixInvalidThisFrame) {
        ++invalidFrameCount;
        ++consecutiveInvalidFrames;
    } else if (consecutiveInvalidFrames > 0) {
        (void)getName();
        consecutiveInvalidFrames = 0;
    }
    if (matrixInvalidThisFrame) (void)getName();
    const auto diagnosticLength = invalidConsecutive.size();
    for (std::size_t i = 0; i < diagnosticLength; ++i) {
        if (!sourcePathAt(invalidIndexThisFrame, i, 541) && sourcePathAt(invalidConsecutive, i, 541) > 0) {
            const auto firstFrame = sourcePathAt(invalidStreakStartFrame, i, 542);
            (void)firstFrame;
            (void)getName();
            (void)sourcePathAt(invalidConsecutive, i, 546);
            (void)sourcePathAt(invalidPerIndex, i, 547);
            sourcePathAt(invalidConsecutive, i, 549) = 0;
            sourcePathAt(invalidStreakStartFrame, i, 550) = 0;
            sourcePathAt(invalidLastLoggedFrame, i, 551) = 0;
            sourcePathAt(invalidLastLoggedCount, i, 552) = 0;
            sourcePathAt(invalidLastLoggedContext, i, 553) = common::NullableString::fromStaticLiteral("");
            sourcePathAt(invalidLastLoggedValueWasNaN, i, 554) = false;
            sourcePathStoreLoggedPoint(invalidLastLoggedValue, i, Vec2{0, 0});
        }
    }
    invalidThisFrame = false;
    matrixInvalidThisFrame = false;
    diagnosticsFrameActive = false;
    if (!invalidThisFrame && !matrixInvalidThisFrame) loggedTransformInvalid = false;
}

void PathDeformer::sourceMarkInvalidOffset(const std::string& context, std::size_t index, Vec2 value) {
    if (!diagnosticsFrameActive) sourceBeginDiagnosticFrame();
    sourceEnsureDiagnosticCapacity(deformation.size());
    invalidThisFrame = true;
    if (index < invalidIndexThisFrame.size()) {
        if (!sourcePathAt(invalidIndexThisFrame, index, 450)) {
            sourcePathAt(invalidIndexThisFrame, index, 451) = true;
            if (sourcePathAt(invalidStreakStartFrame, index, 452) == 0)
                sourcePathAt(invalidStreakStartFrame, index, 453) = frameCounter;
        }
        ++sourcePathAt(invalidPerIndex, index, 456);
        ++sourcePathAt(invalidConsecutive, index, 457);
        const auto consecutive = sourcePathAt(invalidConsecutive, index, 458);
        if (sourcePathAt(invalidStreakStartFrame, index, 459) == 0)
            sourcePathAt(invalidStreakStartFrame, index, 460) = frameCounter;
        const bool previous = sourcePathAt(invalidLastLoggedFrame, index, 480) != 0;
        const bool nonfinite = !guardFinite(value);
        bool changed = !previous;
        if (previous) {
            if (sourcePathAt(invalidLastLoggedValueWasNaN, index, 484) != nonfinite) changed = true;
            else if (!nonfinite) {
                const auto previousValue = sourcePathLoggedPoint(invalidLastLoggedValue, index);
                changed = value.x != previousValue.x || value.y != previousValue.y;
            }
        }
        const bool contextChanged = !previous || sourcePathAt(invalidLastLoggedContext, index, 491) != context;
        const bool consecutiveTrigger = (!previous && consecutive >= 1 && sourcePathAt(invalidPerIndex, index, 492) <= 3) ||
            (consecutive == 10 && sourcePathAt(invalidLastLoggedCount, index, 493) != consecutive) ||
            (consecutive % 10 == 0 && sourcePathAt(invalidLastLoggedCount, index, 494) != consecutive);
        const bool timeTrigger = previous && frameCounter - sourcePathAt(invalidLastLoggedFrame, index, 495) >= 30;
        const bool totalTrigger = sourcePathAt(invalidPerIndex, index, 496) % 10 == 0 &&
            sourcePathAt(invalidLastLoggedFrame, index, 496) != frameCounter;
        if (changed || contextChanged || consecutiveTrigger || timeTrigger || totalTrigger) {
            const auto firstFrame = sourcePathAt(invalidStreakStartFrame, index, 502) != 0 ?
                sourcePathAt(invalidStreakStartFrame, index, 502) : frameCounter;
            (void)firstFrame;
            (void)getName();
            (void)sourcePathFormatVec2(value);
            (void)sourcePathAt(invalidPerIndex, index, 509);
            sourcePathAt(invalidLastLoggedFrame, index, 512) = frameCounter;
            sourcePathAt(invalidLastLoggedCount, index, 513) = consecutive;
            sourcePathAt(invalidLastLoggedContext, index, 514) = context;
            sourcePathStoreLoggedPoint(invalidLastLoggedValue, index, value);
            sourcePathAt(invalidLastLoggedValueWasNaN, index, 516) = !guardFinite(value);
        }
        if (driver && consecutive >= 10) sourceDisablePhysicsDriver(context + ":threshold");
    } else {
        (void)getName();
        (void)sourcePathFormatVec2(value);
    }
}

void PathDeformer::sourceDisablePhysicsDriver(const std::string& context) {
    (void)context;
    if (!driver) return;
    driverInitialized = false;
    prevRootSet = false;
    physicsOnly = false;
    const auto length = deformation.size();
    for (std::size_t i = 0; i < length; ++i) deformation.xAt(i) = 0;
    for (std::size_t i = 0; i < length; ++i) deformation.yAt(i) = 0;
    const auto retainedDriver = driver;
    d_runtime::ngRequireObject(retainedDriver)->reset();
}

void PathDeformer::reportDriverInvalidSource(std::string context, std::size_t index, Vec2 value) {
    (void)context;
    (void)value;
    if (index >= deformation.size()) return;
    deformation.xAt(index) = 0;
    deformation.yAt(index) = 0;
}

Mat4 PathDeformer::sourceRequireFiniteMatrix(Mat4 matrix, const std::string& context) {
    if (!isFiniteMatrix(matrix)) {
        (void)getName();
        (void)context;
        (void)sourcePathMatrixSummary(matrix);
        if (!matrixInvalidThisFrame) ++totalMatrixInvalidCount;
        matrixInvalidThisFrame = true;
        invalidThisFrame = true;
        return Mat4::identity();
    }
    return matrix;
}

Mat4 PathDeformer::sourceSafeInverse(Mat4 matrix, const std::string& context) {
    const auto inverse = Mat4::inverse(matrix);
    if (!isFiniteMatrix(inverse)) {
        (void)getName();
        (void)context;
        (void)isFiniteMatrix(matrix);
        (void)sourcePathMatrixSummary(matrix);
        if (!matrixInvalidThisFrame) ++totalMatrixInvalidCount;
        matrixInvalidThisFrame = true;
        invalidThisFrame = true;
        return Mat4::identity();
    }
    return inverse;
}

SourceDeformResult PathDeformer::deformChildrenSource(std::shared_ptr<Node> target,
    Vec2Array originalVertices, Vec2Array originalDeformation, Mat4* originalTransform) {
    auto profile = core::render::profileScope("PathDeformer.deformChildrenSource");
    const auto retainedReceiver = weak_from_this().lock();
    if (!originalCurve || getVertices().size() < 2) return {{}, nullptr, false};
    const bool started = sourceBeginDiagnosticFrame();
    std::optional<SourceDeformResult> result;
    d_runtime::ngRunWithScopeExit([&] {
        result.emplace([&]() -> SourceDeformResult {
            const auto matrixPointer = d_runtime::ngRequireObject(originalTransform);
            auto centerMatrix = Mat4::multiply(inverseMatrix, *matrixPointer);
            const auto targetName = d_runtime::ngRequireObject(target)->getName();
            centerMatrix = sourceRequireFiniteMatrix(centerMatrix,
                std::string("deformChildren:centerMatrix:") + std::string(targetName.view()));
            // The authored sanitizer changes only offsets, not diagnostic counters.
            for (std::size_t i = 0; i < originalDeformation.size(); ++i) {
                if (!std::isfinite(originalDeformation.xAt(i)) ||
                    !std::isfinite(originalDeformation.yAt(i))) {
                    originalDeformation.xAt(i) = 0;
                    originalDeformation.yAt(i) = 0;
                }
            }
            const auto count = originalVertices.size();
            Vec2Array center, deformed;
            deformed.resize(count);
            if (originalDeformation.size() < count) return {{}, nullptr, false};
            transformAssign(center, originalVertices, centerMatrix);
            transformAdd(center, originalDeformation, centerMatrix);
            const auto read = [](const Vec2Array& values, std::size_t index,
                                 bool y, std::size_t line) {
                if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
                    index, values.size(), d_runtime::RetainedString::fromStaticLiteral(
                        "source/nijilive/core/nodes/deformer/path.d"), line);
                return y ? values.yAt(index) : values.xAt(index);
            };
            const auto invalidCenter = firstNonFiniteIndex(center);
            if (invalidCenter >= 0) {
                const auto index = static_cast<std::size_t>(invalidCenter);
                const Vec2 offset{read(originalDeformation, index, false, 1037),
                    read(originalDeformation, index, true, 1037)};
                sourceMarkInvalidOffset("deformChildren:cVertexNaN", index, offset);
                (void)getName();
                const Vec2 original{read(originalVertices, index, false, 1041),
                    read(originalVertices, index, true, 1041)};
                (void)sourcePathFormatVec2(original);
                (void)sourcePathFormatVec2(offset);
                (void)d_runtime::ngRequireObject(target)->getName();
                return {{}, nullptr, false};
            }
            const auto readCache = [&](std::size_t line) {
                const auto found = meshCaches.find(target);
                if (found == meshCaches.end()) throw d_runtime::ngStaticError<d_runtime::RangeError>(
                    d_runtime::RetainedString::fromStaticLiteral("source/nijilive/core/nodes/deformer/path.d"), line);
                return found->second;
            };
            if (!meshCaches.contains(target) || readCache(1047).size() < count)
                cacheClosestPointsSource(target);
            if (!meshCaches.contains(target) || readCache(1049).size() < count)
                return {{}, nullptr, false};
            common::SharedArray<float> samples(count, std::numeric_limits<float>::quiet_NaN());
            const auto cached = readCache(1055);
            if (count > cached.size()) throw d_runtime::ngStaticError<d_runtime::ArraySliceError>(
                0, count, cached.size(), d_runtime::RetainedString::fromStaticLiteral(
                    "source/nijilive/core/nodes/deformer/path.d"), 1055);
            for (std::size_t i = 0; i < count; ++i) samples[i] = cached[i];
            const auto baseCurve = prevCurve ? prevCurve : originalCurve;
            const auto requireSourceCurve = [](std::shared_ptr<Curve> curve) {
                return d_runtime::ngRequireObject(std::dynamic_pointer_cast<SourceCurveEvaluation>(curve));
            };
            Vec2Array closestOriginal, closestDeformed, tangentOriginalRaw, tangentDeformedRaw;
            requireSourceCurve(baseCurve)->evaluatePointsSource(samples, closestOriginal);
            requireSourceCurve(deformedCurve)->evaluatePointsSource(samples, closestDeformed);
            requireSourceCurve(baseCurve)->evaluateDerivativesSource(samples, tangentOriginalRaw);
            requireSourceCurve(deformedCurve)->evaluateDerivativesSource(samples, tangentDeformedRaw);
#ifdef NIJIGENERATE_DEBUG_PATH_DEFORM
            auto originalSnapshot = closestOriginal.dup();
            if (!closestPointsOriginal) closestPointsOriginal = std::make_shared<ClosestPointsMap>();
            (*closestPointsOriginal)[target] = originalSnapshot;
            auto deformedSnapshot = closestDeformed.dup();
            if (!closestPointsDeformed) closestPointsDeformed = std::make_shared<ClosestPointsMap>();
            (*closestPointsDeformed)[target] = deformedSnapshot;
#endif
            const auto invalid = firstNonFiniteIndex(closestDeformed);
            if (invalid >= 0) {
                const auto index = static_cast<std::size_t>(invalid);
                const Vec2 offset{read(originalDeformation, index, false, 1082),
                    read(originalDeformation, index, true, 1082)};
                sourceMarkInvalidOffset("deformChildren:closestPointDeformedNaN", index, offset);
                (void)getName();
                if (index >= samples.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
                    index, samples.size(), d_runtime::RetainedString::fromStaticLiteral(
                        "source/nijilive/core/nodes/deformer/path.d"), 1086);
                (void)samples[index];
                const Vec2 centerPoint{read(center, index, false, 1087), read(center, index, true, 1087)};
                (void)sourcePathFormatVec2(centerPoint);
                const Vec2 original{read(closestOriginal, index, false, 1088),
                    read(closestOriginal, index, true, 1088)};
                (void)sourcePathFormatVec2(original);
                const Vec2 changed{read(closestDeformed, index, false, 1089),
                    read(closestDeformed, index, true, 1089)};
                (void)sourcePathFormatVec2(changed);
                (void)sourcePathSummarizePoints(deformation);
                return {{}, nullptr, false};
            }
            auto tangentOriginal = tangentOriginalRaw.dup();
            const auto sampleAt = [&](std::size_t index, std::size_t line) {
                if (index >= samples.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
                    index, samples.size(), d_runtime::RetainedString::fromStaticLiteral(
                        "source/nijilive/core/nodes/deformer/path.d"), line);
                return samples[index];
            };
            for (std::size_t i = 0; i < tangentOriginal.size(); ++i) {
                const float squared = tangentOriginal.xAt(i) * tangentOriginal.xAt(i) +
                    tangentOriginal.yAt(i) * tangentOriginal.yAt(i);
                if (squared > 1e-8f) {
                    const float inverse = 1.0f / std::sqrt(squared);
                    tangentOriginal.xAt(i) *= inverse;
                    tangentOriginal.yAt(i) *= inverse;
                } else {
                    tangentOriginal.xAt(i) = 1;
                    tangentOriginal.yAt(i) = 0;
                    (void)getName();
                    (void)sampleAt(i, 1102);
                    (void)sourcePathSummarizePoints(d_runtime::ngRequireObject(baseCurve)->controlPoints());
                }
            }
            auto tangentDeformed = tangentDeformedRaw.dup();
            for (std::size_t i = 0; i < tangentDeformed.size(); ++i) {
                const float squared = tangentDeformed.xAt(i) * tangentDeformed.xAt(i) +
                    tangentDeformed.yAt(i) * tangentDeformed.yAt(i);
                if (squared > 1e-8f) {
                    const float inverse = 1.0f / std::sqrt(squared);
                    tangentDeformed.xAt(i) *= inverse;
                    tangentDeformed.yAt(i) *= inverse;
                } else {
                    tangentDeformed.xAt(i) = read(tangentOriginal, i, false, 1327);
                    tangentDeformed.yAt(i) = read(tangentOriginal, i, true, 1328);
                    (void)getName();
                    (void)sampleAt(i, 1114);
                    (void)sourcePathSummarizePoints(d_runtime::ngRequireObject(deformedCurve)->controlPoints());
                }
            }
            Vec2Array normalOriginal, normalDeformed;
            common::ngRotateVec2TangentsToNormals(normalOriginal, tangentOriginal);
            common::ngRotateVec2TangentsToNormals(normalDeformed, tangentDeformed);
            common::SharedArray<float> normalDistances(count, std::numeric_limits<float>::quiet_NaN());
            common::SharedArray<float> tangentialDistances(count, std::numeric_limits<float>::quiet_NaN());
            common::ngProjectVec2OntoAxes(center, closestOriginal, normalOriginal, tangentOriginal,
                normalDistances, tangentialDistances);
            common::ngComposeVec2FromAxes(deformed, closestDeformed, normalDistances, normalDeformed,
                tangentialDistances, tangentDeformed);
            const auto inverseName = d_runtime::ngRequireObject(target)->getName();
            auto inverse = sourceSafeInverse(centerMatrix,
                std::string("deformChildren:centerInverse:") + std::string(inverseName.view()));
            inverse[0][3] = 0;
            inverse[1][3] = 0;
            inverse[2][3] = 0;
            auto local = deformed.dup();
            sourcePathSubtract(local, center);
            transformAdd(originalDeformation, local, inverse, local.size());
            if (getDriverSource()) d_runtime::ngRequireObject(target)->notifyChange(target);
            return {originalDeformation, nullptr, true};
        }());
    }, [&] { if (started) sourceEndDiagnosticFrame(); });
    return std::move(*result);
}

DeformResult PathDeformer::deformChildren(const std::shared_ptr<Node>& target,
                                          const Vec2Array& origVertices,
                                          Vec2Array& origDeformation,
                                          const Mat4* origTransform) {
    auto scope = core::render::profileScope("PathDeformer.deformChildren");
    static std::unordered_map<uint32_t, uint64_t> sChangedCount;
    static uint64_t sSummaryTick = 0;
    DeformResult res;

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

    Vec2Array& sample = sampleScratch_;
    {
        auto transformScope = core::render::profileScope("PathDeformer.sampleTransform");
        sample.resize(origVertices.size());
        const float c00 = center[0][0];
        const float c01 = center[0][1];
        const float c03 = center[0][3];
        const float c10 = center[1][0];
        const float c11 = center[1][1];
        const float c13 = center[1][3];
        const std::size_t overlap = std::min(sample.size(), origDeformation.size());
        for (std::size_t i = 0; i < sample.size(); ++i) {
            const float vertexX = origVertices.xAt(i);
            const float vertexY = origVertices.yAt(i);
            float sampleX = vertexX * c00 + vertexY * c01 + c03;
            float sampleY = vertexX * c10 + vertexY * c11 + c13;
            if (i < overlap) {
                float deformX = origDeformation.xAt(i);
                float deformY = origDeformation.yAt(i);
                if (!std::isfinite(deformX) || !std::isfinite(deformY)) {
                    deformX = 0.0f;
                    deformY = 0.0f;
                    origDeformation.xAt(i) = 0.0f;
                    origDeformation.yAt(i) = 0.0f;
                    recordInvalid("sanitize", i, Vec2{0, 0});
                }
                sampleX += deformX * c00 + deformY * c01;
                sampleY += deformX * c10 + deformY * c11;
            }
            sample.xAt(i) = sampleX;
            sample.yAt(i) = sampleY;
            if (!std::isfinite(sampleX) || !std::isfinite(sampleY)) {
                markInvalidOffset("cVertexNaN", i, Vec2{});
                if (diagStarted) endDiagnosticFrame();
                return res;
            }
        }
    }

    // D parity: only reuse cached t-samples when entry already exists and has enough vertices.
    const auto tgtKey = target;
    auto it = meshCaches.find(tgtKey);
    bool hasValidCache = (it != meshCaches.end() && it->second.size() >= sample.size());
    if (!hasValidCache) {
        cacheClosestPoints(target);
        it = meshCaches.find(tgtKey);
        hasValidCache = (it != meshCaches.end() && it->second.size() >= sample.size());
    }
    if (!hasValidCache) {
        if (diagStarted) endDiagnosticFrame();
        return res;
    }
    const auto cache = it->second;
    // Compatibility for the unchanged native vector-based Curve overload.
    // The Source value-descriptor path consumes the real cache directly.
    tSamplesScratch_.assign(cache.begin(), cache.begin() + static_cast<std::ptrdiff_t>(sample.size()));
    const std::vector<float>* tSamples = &tSamplesScratch_;

    Curve* baseCurve = prevCurve ? prevCurve.get() : originalCurve.get();
    Vec2Array& closestOrig = closestOrigScratch_;
    Vec2Array& closestDef = closestDefScratch_;
    Vec2Array& tangentOrig = tangentOrigScratch_;
    Vec2Array& tangentDef = tangentDefScratch_;
    {
        auto curveScope = core::render::profileScope("PathDeformer.curveEvaluate");
        if (baseCurve) baseCurve->evaluatePoints(*tSamples, closestOrig);
        deformedCurve->evaluatePoints(*tSamples, closestDef);
        if (baseCurve) baseCurve->evaluateDerivatives(*tSamples, tangentOrig);
        deformedCurve->evaluateDerivatives(*tSamples, tangentDef);
    }
#ifdef NIJIGENERATE_DEBUG_PATH_DEFORM
    // Independent path.d records both snapshots before validating closestDef.
    auto originalSnapshot = closestOrig.dup();
    if (!closestPointsOriginal) closestPointsOriginal = std::make_shared<ClosestPointsMap>();
    (*closestPointsOriginal)[target] = std::move(originalSnapshot);
    auto deformedSnapshot = closestDef.dup();
    if (!closestPointsDeformed) closestPointsDeformed = std::make_shared<ClosestPointsMap>();
    (*closestPointsDeformed)[target] = std::move(deformedSnapshot);
#endif

    float sumOffset = 0.0f;
    {
        auto composeScope = core::render::profileScope("PathDeformer.composeOffsets");
        for (std::size_t i = 0; i < sample.size(); ++i) {
            const float sampleX = sample.xAt(i);
            const float sampleY = sample.yAt(i);
            const float closestOrigX = closestOrig.xAt(i);
            const float closestOrigY = closestOrig.yAt(i);
            const float closestDefX = closestDef.xAt(i);
            const float closestDefY = closestDef.yAt(i);
            if (!std::isfinite(closestDefX) || !std::isfinite(closestDefY)) {
                markInvalidOffset("closestPointDeformedNaN", i, Vec2{});
                if (diagStarted) endDiagnosticFrame();
                return res;
            }
            float tangentOrigX = tangentOrig.xAt(i);
            float tangentOrigY = tangentOrig.yAt(i);
            const float tangentOrigLenSq = tangentOrigX * tangentOrigX + tangentOrigY * tangentOrigY;
            if (tangentOrigLenSq > 1e-8f) {
                const float invOrigLen = 1.0f / std::sqrt(tangentOrigLenSq);
                tangentOrigX *= invOrigLen;
                tangentOrigY *= invOrigLen;
            } else {
                tangentOrigX = 1.0f;
                tangentOrigY = 0.0f;
            }
            float tangentDefX = tangentDef.xAt(i);
            float tangentDefY = tangentDef.yAt(i);
            const float tangentDefLenSq = tangentDefX * tangentDefX + tangentDefY * tangentDefY;
            if (tangentDefLenSq > 1e-8f) {
                const float invDefLen = 1.0f / std::sqrt(tangentDefLenSq);
                tangentDefX *= invDefLen;
                tangentDefY *= invDefLen;
            } else {
                tangentDefX = tangentOrigX;
                tangentDefY = tangentOrigY;
            }
            const float relativeX = sampleX - closestOrigX;
            const float relativeY = sampleY - closestOrigY;
            const float normalDistance = relativeX * (-tangentOrigY) + relativeY * tangentOrigX;
            const float tangentialDistance = relativeX * tangentOrigX + relativeY * tangentOrigY;
            float offsetX = closestDefX + (-tangentDefY) * normalDistance + tangentDefX * tangentialDistance - sampleX;
            float offsetY = closestDefY + tangentDefX * normalDistance + tangentDefY * tangentialDistance - sampleY;
            if (!std::isfinite(offsetX) || !std::isfinite(offsetY)) {
                offsetX = 0.0f;
                offsetY = 0.0f;
                recordInvalid("offsetNaN", i, Vec2{0, 0});
            }
            sample.xAt(i) = offsetX;
            sample.yAt(i) = offsetY;
            float mag = std::sqrt(offsetX * offsetX + offsetY * offsetY);
            sumOffset += mag;
            lastMaxOffset = std::max(lastMaxOffset, mag);
        }
    }
    if (sample.size() > 0) lastAvgOffset = sumOffset / static_cast<float>(sample.size());

    Mat4 inv = Mat4::inverse(center);
    if (!isFiniteMatrix(inv)) {
        recordInvalid("centerInverse", 0, Vec2{});
        disablePhysicsDriver("centerInverse");
        if (diagStarted) endDiagnosticFrame();
        return res;
    }
    inv[0][3] = inv[1][3] = inv[2][3] = 0.0f;
    float maxAbs = 0.0f;
    {
        auto writebackScope = core::render::profileScope("PathDeformer.writebackOffsets");
        if (origDeformation.size() < sample.size()) {
            origDeformation.resize(sample.size());
        }
        const float i00 = inv[0][0];
        const float i01 = inv[0][1];
        const float i10 = inv[1][0];
        const float i11 = inv[1][1];
        for (std::size_t i = 0; i < sample.size(); ++i) {
            float baseX = origDeformation.xAt(i);
            float baseY = origDeformation.yAt(i);
            if (!std::isfinite(baseX) || !std::isfinite(baseY)) {
                recordInvalid("resultNaN", i, Vec2{baseX, baseY});
                baseX = 0.0f;
                baseY = 0.0f;
            }
            float outX = baseX + sample.xAt(i) * i00 + sample.yAt(i) * i01;
            float outY = baseY + sample.xAt(i) * i10 + sample.yAt(i) * i11;
            if (!std::isfinite(outX) || !std::isfinite(outY)) {
                recordInvalid("resultNaN", i, Vec2{outX, outY});
                outX = 0.0f;
                outY = 0.0f;
            }
            origDeformation.xAt(i) = outX;
            origDeformation.yAt(i) = outY;
            maxAbs = std::max(maxAbs, std::max(std::fabs(outX), std::fabs(outY)));
        }
    }
    res.changed = true;
    if (maxAbs > 10.0f) {
        pathLog("[PathDeformer][LargeOffset] node=", name,
                " target=", (target ? target->name : common::NullableString::fromStaticLiteral("<null>")),
                " targetUuid=", (target ? target->uuid : 0u),
                " maxAbs=", maxAbs,
                " first=(", (origDeformation.empty() ? 0.0f : origDeformation.xAt(0)),
                ",", (origDeformation.empty() ? 0.0f : origDeformation.yAt(0)), ")");
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
    const auto lifetime = weak_from_this().lock();
    if (diagnosticsFrameActive) sourceEndDiagnosticFrame();
    const bool started = sourceBeginDiagnosticFrame();
    const auto current = transform();
    if (!isFiniteMatrix(current.toMat4())) {
        sourceLogTransformFailure("runPreProcessTask:transform", current);
        sourceDisablePhysicsDriver("transformInvalid");
        // The authored early return deliberately leaves the active frame.
        return;
    }
    sourceRefreshInverseMatrix("runPreProcessTask:initial");
    const auto original = deformation.dup();
    Deformable::runPreProcessTask(ctx);
    sourceApplyPathDeform(original);
    if (started) sourceEndDiagnosticFrame();
}

void PathDeformer::sourceApplyPathDeform(const Vec2Array original) {
    (void)transform();
    sourceRefreshInverseMatrix("applyPathDeform:pre");
    sourceSanitizeOffsets(deformation);
    const auto enabled = [&] {
        if (!getDriverSource()) return false;
        if (!puppetRef()) return false;
        return d_runtime::ngRequireObject(puppetRef())->enableDrivers;
    };
    const auto requireDriver = [&] { return d_runtime::ngRequireObject(getDriverSource()); };
    if (getDriverSource()) {
        auto baseline = original.size() == deformation.size() ? original.dup() : deformation.dup();
        sourceSanitizeOffsets(baseline);
        if (!driverInitialized && enabled()) {
            requireDriver()->setup();
            if (!getDriverSource()) {
                sourceHandleInvalidDeformation("applyPathDeform:setupDisabled", original);
                return;
            }
            driverInitialized = true;
        }
        auto difference = deformation.dup();
        sourcePathSubtract(difference, baseline);
        sourceSanitizeOffsets(difference);
        if (getVertices().size() >= 2) {
            auto previous = getVertices().dup();
            sourcePathAdd(previous, difference);
            prevCurve = createCurve(previous);
            clearCache();
            (void)sourceLogCurveHealth("applyPathDeform:driverPrev", originalCurve, prevCurve, deformation);
            if (enabled()) requireDriver()->updateDefaultShape();
            if (!getDriverSource()) {
                sourceHandleInvalidDeformation("applyPathDeform:updateDefaultShapeDisabled", original);
                return;
            }
            if (enabled()) {
                Vec2 root;
                const auto matrix = sourceRequireFiniteMatrix(transform().toMat4(), "applyPathDeform:transform");
                if (!deformation.empty()) {
                    const auto& points = getVertices();
#ifndef NDEBUG
                    if (points.empty() || deformation.empty()) throw d_runtime::ngStaticError<d_runtime::AssertError>(
                        "veca index out of range", "source/nijilive/math/veca.d", 139);
#endif
                    const auto vertex = sourcePathFirst(points);
                    const auto offset = sourcePathFirst(deformation);
                    const auto position = sourcePathMatrixVector(matrix,
                        {vertex.x + offset.x, vertex.y + offset.y, 0, 1});
                    root = sanitizeVec2({position.x, position.y});
                } else root = {0, 0};
                if (prevRootSet) {
                    const auto force = sanitizeVec2({root.x - prevRoot.x, root.y - prevRoot.y});
                    requireDriver()->reset();
                    requireDriver()->enforce(force);
                    requireDriver()->rotate(transform().rotation.z);
                    if (physicsOnly) {
                        const auto previousDeformation = deformation.dup();
                        requireDriver()->update(deformation, strength, frameCounter);
                        if (!getDriverSource()) {
                            sourceHandleInvalidDeformation("applyPathDeform:updateDisabled", original);
                            return;
                        }
                        prevCurve = createCurve(getVertices());
                        (void)sourceLogCurveHealth("applyPathDeform:physicsPrev", originalCurve, prevCurve, deformation);
                        sourcePathSubtract(deformation, previousDeformation);
                        sourceSanitizeOffsets(deformation);
                    } else requireDriver()->update(deformation, strength, frameCounter);
                    if (!getDriverSource()) {
                        sourceHandleInvalidDeformation("applyPathDeform:updateDisabled", original);
                        return;
                    }
                }
                prevRoot = root;
                prevRootSet = true;
            }
            auto candidate = getVertices().dup();
            sourcePathAdd(candidate, deformation);
            sourceSanitizeOffsets(candidate);
            deformSource(candidate);
            sourceLogCurveState("applyPathDeform");
        }
        sourceRefreshInverseMatrix("applyPathDeform:postDriver");
    } else {
        if (getVertices().size() >= 2) {
            auto candidate = getVertices().dup();
            sourcePathAdd(candidate, deformation);
            sourceSanitizeOffsets(candidate);
            deformSource(candidate);
            sourceLogCurveState("applyPathDeform");
        }
        sourceRefreshInverseMatrix("applyPathDeform:postNoDriver");
    }
    updateDeform();
}

/* Native compatibility kernel; Source lifecycle calls sourceApplyPathDeform. */
void PathDeformer::applyPathDeform(const Vec2Array& origDeform) {
    auto scope = core::render::profileScope("PathDeformer.applyPathDeform");
    transform();
    refreshInverseMatrix("applyPathDeform:pre");
    sanitizeOffsets(deformation);
    auto logMaxDeform = [&](const char* phase) {
        float maxAbs = 0.0f;
        for (std::size_t i = 0; i < deformation.size(); ++i) {
            maxAbs = std::max(maxAbs, std::fabs(deformation.xAt(i)));
            maxAbs = std::max(maxAbs, std::fabs(deformation.yAt(i)));
        }
        if (maxAbs > 5.0f) {
            pathLog("[PathDeformer][DeformMag] node=", name,
                    " phase=", phase,
                    " maxAbs=", maxAbs,
                    " count=", deformation.size());
        }
    };
    logMaxDeform("pre");
    auto pup = puppetRef();
    const bool enableDriverStep = (pup && pup->enableDrivers);
    if (driver) {
        Vec2Array baseline = (origDeform.size() == deformation.size()) ? origDeform.dup() : deformation.dup();
        sanitizeOffsets(baseline);
        if (!driverInitialized && enableDriverStep) {
            driver->setup();
            driverInitialized = true;
        }
        Vec2Array diffDeform = deformation.dup();
        if (diffDeform.size() == baseline.size()) {
            for (std::size_t i = 0; i < diffDeform.size(); ++i) {
                diffDeform.xAt(i) -= baseline.xAt(i);
                diffDeform.yAt(i) -= baseline.yAt(i);
            }
        }
        sanitizeOffsets(diffDeform);

        if (vertices.size() >= 2) {
            auto prevCandidate = vertices.dup();
            prevCandidate += diffDeform;
            prevCurve = nicxlive::core::nodes::createCurve(prevCandidate, curveType == CurveType::Bezier);
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
                        Vec2Array prevDeform = deformation.dup();
                        driver->update(deformation, strength, frameCounter);
                        prevCurve = nicxlive::core::nodes::createCurve(vertices, curveType == CurveType::Bezier);
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

            auto candidate = vertices.dup();
            candidate += deformation;
            sanitizeOffsets(candidate);
            deform(candidate);
            logMaxDeform("postDriver");
            logCurveState("applyPathDeform");
        }
        refreshInverseMatrix("applyPathDeform:postDriver");
    } else {
        if (vertices.size() >= 2) {
            auto candidate = vertices.dup();
            candidate += deformation;
            sanitizeOffsets(candidate);
            deform(candidate);
            logMaxDeform("postNoDriver");
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
    deformedCurve = nicxlive::core::nodes::createCurve(deformedControlPoints, curveType == CurveType::Bezier);
}

void PathDeformer::runRenderTask(core::RenderContext&) {
    // no GPU commands
}

void PathDeformer::runDynamicTask(core::RenderContext& context) {
    Deformable::runDynamicTask(context);
}

void PathDeformer::centralize() {
    const auto lifetime = weak_from_this().lock();
    const auto initial = children;
    for (auto child : initial) d_runtime::ngRequireObject(child)->centralize();
    std::array<float, 4> bounds{};
    std::vector<Vec4> translations;
    if (!children.empty()) {
        const auto first = children[0];
        bounds = d_runtime::ngRequireObject(first)->getCombinedBounds();
        const auto range = children;
        for (auto child : range) {
            const auto current = d_runtime::ngRequireObject(child)->getCombinedBounds();
            bounds[0] = std::min(bounds[0], current[0]);
            bounds[1] = std::min(bounds[1], current[1]);
            bounds[2] = std::max(bounds[2], current[2]);
            bounds[3] = std::max(bounds[3], current[3]);
            translations.push_back(sourcePathMatrixVector(d_runtime::ngRequireObject(child)->transform().toMat4(),
                Vec4{0, 0, 0, 1}));
        }
    } else {
        const auto translation = transform().translation;
        bounds = {translation.x, translation.y, translation.x, translation.y};
    }
    (void)bounds;
    transformChanged();
    const auto range = children;
    for (std::size_t i = 0; i < range.size(); ++i) {
        const auto child = d_runtime::ngRequireObject(range[i]);
        auto& destination = child->localTransform.translation;
        const auto inverse = Mat4::inverse(transform().toMat4());
#ifndef NDEBUG
        if (i >= translations.size()) throw d_runtime::ngStaticError<d_runtime::AssertError>(
            "veca index out of range", "source/nijilive/math/veca.d", 139);
#endif
        if (i >= translations.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
            i, translations.size(), d_runtime::RetainedString::fromStaticLiteral(
                "source/nijilive/math/veca.d"), 467);
        const auto position = sourcePathMatrixVector(inverse, translations[i]);
        destination = {position.x, position.y, position.z};
        child->transformChanged();
    }
}

void PathDeformer::switchDynamic(bool dynamic) {
    const auto lifetime = weak_from_this().lock();
    if (dynamic != dynamicDeformation) {
        dynamicDeformation = dynamic;
        build();
    }
}

void PathDeformer::notifyChange(const std::shared_ptr<Node>& target, NotifyReason reason) {
    const auto value = target;
    const auto receiver = d_runtime::ngRequireObject(weak_from_this().lock());
    if (!common::ngClassObjectEquals(value, receiver) && reason == NotifyReason::StructureChanged) {
        if (meshCaches.contains(value)) meshCaches.erase(value);
    }
    Deformable::notifyChange(value, reason);
}

bool PathDeformer::setupChild(const std::shared_ptr<Node>& child) {
    const auto value = child;
    Deformable::setupChild(value);
    std::function<void(std::shared_ptr<Node>)> setGroup;
    setGroup = [&](std::shared_ptr<Node> node) {
        setupChildNoRecurse(node);
        if (d_runtime::ngRequireObject(node)->mustPropagate()) {
            const auto range = node->childrenRef();
            for (auto c : range) setGroup(c);
        }
    };
    setGroup(value);
    return true;
}

bool PathDeformer::releaseChild(const std::shared_ptr<Node>& child) {
    const auto value = child;
    std::function<void(std::shared_ptr<Node>)> unsetGroup;
    unsetGroup = [&](std::shared_ptr<Node> node) {
        releaseChildNoRecurse(node);
        if (!d_runtime::ngRequireObject(node)->mustPropagate()) {
            const auto range = node->childrenRef();
            for (auto c : range) unsetGroup(c);
        }
    };
    unsetGroup(value);
    Deformable::releaseChild(value);
    return true;
}

void PathDeformer::captureTarget(const std::shared_ptr<Node>& target) {
    const auto value = target;
    childrenRef().push_back(value);
    setupChildNoRecurse(value, true);
}

void PathDeformer::releaseTarget(const std::shared_ptr<Node>& target) {
    const auto value = target;
    releaseChildNoRecurse(value);
    auto range = childrenRef();
    for (std::size_t i = 0; i < range.size(); ++i) {
        const auto candidate = range[i];
        if (common::ngClassObjectEquals(candidate, value)) {
            range.erase(range.begin() + i);
            break;
        }
    }
    childrenRef() = range;
}

void PathDeformer::clearCache() {
    meshCaches.clear();
}

void PathDeformer::dispose() {
    const auto lifetime = weak_from_this().lock();
    const auto range = children;
    for (auto child : range) releaseChild(child);
    childrenRef().resize(0);
}

void PathDeformer::build(bool force) {
    const auto range = children;
    for (auto c : range) setupChild(c);
    setupSelf();
    Deformable::build(force);
}

bool PathDeformer::setupChildNoRecurse(std::shared_ptr<Node> node, bool prepend) {
    const auto value = node;
    const auto receiver = d_runtime::ngRequireObject(std::dynamic_pointer_cast<PathDeformer>(weak_from_this().lock()));
    const auto drawable = std::dynamic_pointer_cast<Drawable>(value);
    const auto grid = std::dynamic_pointer_cast<GridDeformer>(value);
    const bool supportsDeform = static_cast<bool>(drawable) || static_cast<bool>(grid);
    if (supportsDeform) {
        cacheClosestPointsSource(value);
        if (dynamicDeformation) {
            auto& post = d_runtime::ngRequireObject(value)->postProcessFilters;
            post = sourcePathUpsertFilter(post, receiver, prepend);
            auto& pre = d_runtime::ngRequireObject(value)->preProcessFilters;
            pre = sourcePathRemoveFilter(pre, receiver);
        } else {
            auto& pre = d_runtime::ngRequireObject(value)->preProcessFilters;
            pre = sourcePathUpsertFilter(pre, receiver, prepend);
            auto& post = d_runtime::ngRequireObject(value)->postProcessFilters;
            post = sourcePathRemoveFilter(post, receiver);
        }
    } else {
        meshCaches.erase(value);
        auto& pre = d_runtime::ngRequireObject(value)->preProcessFilters;
        pre = sourcePathRemoveFilter(pre, receiver);
        auto& post = d_runtime::ngRequireObject(value)->postProcessFilters;
        post = sourcePathRemoveFilter(post, receiver);
    }
    return true;
}

bool PathDeformer::releaseChildNoRecurse(std::shared_ptr<Node> node) {
    const auto value = node;
    const auto receiver = d_runtime::ngRequireObject(std::dynamic_pointer_cast<PathDeformer>(weak_from_this().lock()));
    meshCaches.erase(value);
    auto& pre = d_runtime::ngRequireObject(value)->preProcessFilters;
    pre = sourcePathRemoveFilter(pre, receiver);
    auto& post = d_runtime::ngRequireObject(value)->postProcessFilters;
    post = sourcePathRemoveFilter(post, receiver);
    return true;
}

void PathDeformer::applyDeformToChildren(common::SharedArray<std::shared_ptr<core::param::Parameter>> params, bool recursive) {
    const auto receiver = d_runtime::ngRequireObject(std::dynamic_pointer_cast<PathDeformer>(weak_from_this().lock()));
    if (getDriverSource()) {
        physicsOnly = true;
        return;
    }
    if (dynamicDeformation) return;
    const bool started = sourceBeginDiagnosticFrame();
    d_runtime::ngRunWithScopeExit([&] {
        NodeFilter::SourceUpdate update([receiver](Vec2Array values) {
            receiver->sourceSanitizeOffsets(values);
            if (receiver->getVertices().size() >= 2) {
                auto candidate = receiver->getVertices().dup();
                sourcePathAdd(candidate, values);
                receiver->sourceSanitizeOffsets(candidate);
                receiver->deformSource(candidate);
                receiver->sourceLogCurveState("applyDeformToChildren");
            }
            receiver->sourceRefreshInverseMatrix("applyDeformToChildren:update");
        });
        NodeFilter::SourceTransferCondition transfer([] { return false; });
        _applyDeformToChildren(sourcePathFilter(receiver), update, transfer, params, recursive);
        physicsOnly = true;
        rebuffer(Vec2Array{});
    }, [&] { if (started) sourceEndDiagnosticFrame(); });
}

void PathDeformer::copyFrom(const Node& src, bool clone, bool deepCopy) {
    Deformable::copyFrom(src, clone, deepCopy);
    if (auto path = dynamic_cast<const PathDeformer*>(&src)) {
        clearCache();
        curveType = path->curveType;
        physicsType = path->physicsType;
        physicsOnly = path->physicsOnly;
          if (const_cast<PathDeformer*>(path)->getDriverSource()) {
              setDriverSource(createPhysicsSource());
        }
        driverInitialized = false;
    }
    if (auto deformable = dynamic_cast<const Deformable*>(&src)) {
        originalCurve = createCurve(const_cast<Deformable*>(deformable)->getVertices());
    }
    deformation.resize(d_runtime::ngRequireObject(originalCurve)->controlPoints().size());
}

void PathDeformer::rebuffer(const std::vector<Vec2>& points) {
    rebuffer(toVec2Array(points));
}

void PathDeformer::rebuffer(const Vec2Array& points) {
    const auto values = points;
    originalCurve = createCurve(values);
    deformedCurve = createCurve(values);
    deformation.resize(values.size());
    clearCache();
    driverInitialized = false;
    prevCurve = originalCurve;
    sourceEnsureDiagnosticCapacity(deformation.size());
    sourceCheckBaselineDegeneracy(d_runtime::ngRequireObject(originalCurve)->controlPoints());
    sourceLogCurveState("rebuffer");
}

void PathDeformer::serializeSelfImpl(core::serde::TypedJsonWriter& writer,
    bool recursive, SerializeNodeFlags flags) const {
    const auto lifetime = weak_from_this().lock();
    Deformable::serializeSelfImpl(writer, recursive, flags);
    if (physicsOnly) {
        writer.putKey("physics_only");
        writer.serializeValue(physicsOnly);
    }
    writer.putKey("curve_type");
    switch (curveType) {
    case CurveType::Bezier: writer.putValue("Bezier"); break;
    case CurveType::Spline: writer.putValue("Spline"); break;
    default:
        d_runtime::ngAssertUnreachable("Assertion failure",
            "source/mir/serde.d", 522);
    }
    if (has_flag(flags, SerializeNodeFlags::Geometry)) {
        writer.putKey("vertices");
        const auto state = writer.listBegin();
        if (originalCurve) {
            const auto curve = d_runtime::ngRequireObject(originalCurve);
            auto& points = curve->controlPoints();
            const auto count = points.size();
            for (std::size_t i = 0; i < count; ++i) {
                writer.elemBegin(); writer.serializeValue(points.xAt(i));
                writer.elemBegin(); writer.serializeValue(points.yAt(i));
            }
        }
        writer.listEnd(state);
    }
    if (has_flag(flags, SerializeNodeFlags::Links) && driver) {
        writer.putKey("physics");
        const auto value = driver;
        if (value) value->serializeSource(writer);
        else writer.serializeValue(nullptr);
    }
    writer.putKey("dynamic_deformation");
    writer.serializeValue(dynamicDeformation);
}

core::serde::ReadError PathDeformer::deserializeFromFghj(core::serde::TypedJsonView data) {
    const auto lifetime = weak_from_this().lock();
    // path.d627 deliberately ignores the returned superclass exception.
    (void)Deformable::deserializeFromFghj(data);
    const auto only = data.findFirst("physics_only");
    if (!only.isMissing())
        if (auto error = core::serde::deserializeValue(only, physicsOnly)) return error;
    const auto type = data.findFirst("curve_type");
    if (!type.isMissing()) {
        if (type.kind() != core::serde::JsonKind::String && type.kind() != core::serde::JsonKind::Null)
            (void)core::serde::unexpectedJsonKind(type);
        const auto text = type.kind() == core::serde::JsonKind::String
            ? type.stringBytes() : std::string_view{};
        if (text == "Bezier") curveType = CurveType::Bezier;
        else if (text == "Spline") curveType = CurveType::Spline;
        else throw core::serde::JsonConversionError(d_runtime::RetainedString(
            "Unable to deserialize string '" + std::string(text) +
            "' to CurveTypeAllowed keys:allowedKeys"),
            d_runtime::RetainedString::fromStaticLiteral("source/fghj/serialization.d"), 2167);
    }
    dynamicDeformation = false;
    const auto dynamic = data.findFirst("dynamic_deformation");
    if (!dynamic.isMissing())
        if (auto error = core::serde::deserializeValue(dynamic, dynamicDeformation)) return error;
    auto elements = data.findFirst("vertices").byElement();
    Vec2Array points;
    while (!elements.empty()) {
        float x = std::numeric_limits<float>::quiet_NaN();
        float y = std::numeric_limits<float>::quiet_NaN();
        (void)core::serde::deserializeValue(elements.front(), x);
        elements.popFront();
        (void)core::serde::deserializeValue(elements.front(), y);
        elements.popFront();
        points.append(Vec2{x, y});
    }
    rebuffer(points);
    const auto physics = data.findFirst("physics");
    if (!physics.isMissing()) {
        d_runtime::RetainedString name;
        (void)core::serde::deserializeValue(physics.findFirst("type"), name);
        const auto receiver = std::dynamic_pointer_cast<PathDeformer>(
            d_runtime::ngRequireObject(weak_from_this().lock()));
        if (name.view() == "Pendulum") {
            auto value = std::make_shared<ConnectedDriverAdapter>(
                std::make_unique<ConnectedPendulumDriver>(receiver), receiver.get());
            (void)value->deserializeSource(physics);
            setDriverSource(value);
        } else if (name.view() == "SpringPendulum") {
            auto value = std::make_shared<ConnectedDriverAdapter>(
                std::make_unique<ConnectedSpringPendulumDriver>(receiver), receiver.get());
            (void)value->deserializeSource(physics);
            setDriverSource(value);
        }
    }
    return {};
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
