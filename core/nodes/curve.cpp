#include "curve.hpp"
#include "../math/simd.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cmath>
#include <functional>
#include <limits>

namespace nicxlive::core::nodes {

using ::nicxlive::core::common::Vec2Array;

namespace {
Vec2 sourceCurveFirst(Vec2Array& values) {
#ifndef NDEBUG
    if (values.empty()) throw d_runtime::ngStaticError<d_runtime::AssertError>(
        "veca index out of range", "source/nijilive/math/veca.d", 139);
#endif
    if (values.empty()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
        0, 0, d_runtime::RetainedString::fromStaticLiteral("source/nijilive/math/veca.d"), 467);
    return {values.xAt(0), values.yAt(0)};
}

float sourceCurveBinomial(std::int64_t n, std::int64_t k) {
    if (k > n) return 0;
    if (k == 0 || k == n) return 1;
    const auto remaining = std::bit_cast<std::int64_t>(
        static_cast<std::uint64_t>(n) - static_cast<std::uint64_t>(k));
    k = k > remaining ? remaining : k;
    float result = 1;
    // The original counter is int, while its n/k arithmetic is long.
    for (std::uint32_t bits = 0; std::bit_cast<std::int32_t>(bits) < k; ++bits) {
        const auto i = std::bit_cast<std::int32_t>(bits);
        const auto numerator = std::bit_cast<std::int64_t>(
            static_cast<std::uint64_t>(n) - static_cast<std::uint64_t>(static_cast<std::int64_t>(i)));
        result *= static_cast<float>(numerator) /
            static_cast<float>(std::bit_cast<std::int32_t>(bits + std::uint32_t{1}));
    }
    return result;
}

void sourceBezierEvaluate(const Vec2Array& basis, const common::SharedArray<float> samples,
                          Vec2Array& dst) {
    dst.resize(samples.size());
    const auto count = basis.size();
    if (samples.empty() || count == 0) return;
    const auto degree = std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(count - 1));
    std::vector<std::array<float, 4>> powers(count), onePowers(count);
    std::vector<float> scalarPowers(count), scalarOne(count);
    std::size_t idx = 0;
    for (; idx + 4 <= samples.size(); idx += 4) {
        const auto tVector = math::simd::ngLoadVecSource(samples, idx);
        const auto oneVector = math::simd::ngSplatSimdSource(1.0f) - tVector;
        const auto& ts = tVector.scalars;
        const auto& one = oneVector.scalars;
        std::array<float, 4> xs{}, ys{};
        powers[0] = math::simd::ngSplatSimdSource(1.0f).scalars;
        onePowers[0] = math::simd::ngSplatSimdSource(1.0f).scalars;
        for (std::size_t i = 1; i < count; ++i) for (std::size_t lane = 0; lane < 4; ++lane) {
            powers[i][lane] = powers[i - 1][lane] * ts[lane];
            onePowers[i][lane] = onePowers[i - 1][lane] * one[lane];
        }
        for (std::size_t i = 0; i < count; ++i) {
            const auto binomial = sourceCurveBinomial(degree, static_cast<std::int64_t>(i));
            const float x = basis.xAt(i), y = basis.yAt(i);
            for (std::size_t lane = 0; lane < 4; ++lane) {
                const float coefficient = binomial * onePowers[count - 1 - i][lane] * powers[i][lane];
                xs[lane] += coefficient * x; ys[lane] += coefficient * y;
            }
        }
        // Read every sample/control contribution before either four-lane store.
        math::simd::ngStoreVecSource(std::span<float>(dst.dataXMutable(), dst.size()), idx,
            math::simd::FloatSimd{xs});
        math::simd::ngStoreVecSource(std::span<float>(dst.dataYMutable(), dst.size()), idx,
            math::simd::FloatSimd{ys});
    }
    for (; idx < samples.size(); ++idx) {
        const float t = samples[idx], one = 1 - t;
        float x = 0, y = 0;
        scalarPowers[0] = 1; scalarOne[0] = 1;
        for (std::size_t i = 1; i < count; ++i) {
            scalarPowers[i] = scalarPowers[i - 1] * t;
            scalarOne[i] = scalarOne[i - 1] * one;
        }
        for (std::size_t i = 0; i < count; ++i) {
            const float coefficient = sourceCurveBinomial(degree, static_cast<std::int64_t>(i))
                * scalarOne[count - 1 - i] * scalarPowers[i];
            x += coefficient * basis.xAt(i); y += coefficient * basis.yAt(i);
        }
        dst.xAt(idx) = x; dst.yAt(idx) = y;
    }
}

template<bool Derivative> void sourceSplineEvaluate(const Vec2Array& points,
    const common::SharedArray<float> samples, Vec2Array& dst) {
    dst.resize(samples.size());
    if (samples.empty()) return;
    const auto len = points.size();
    if (len < 2) {
        for (std::size_t i = 0; i < samples.size(); ++i) dst.xAt(i) = 0;
        for (std::size_t i = 0; i < samples.size(); ++i) dst.yAt(i) = 0;
        return;
    }
    std::size_t idx = 0;
    if (len == 2) {
        if constexpr (Derivative) {
            const float dx = points.xAt(1) - points.xAt(0), dy = points.yAt(1) - points.yAt(0);
            for (; idx + 4 <= samples.size(); idx += 4) {
                for (std::size_t lane = 0; lane < 4; ++lane) dst.xAt(idx + lane) = dx;
                for (std::size_t lane = 0; lane < 4; ++lane) dst.yAt(idx + lane) = dy;
            }
            for (; idx < samples.size(); ++idx) { dst.xAt(idx) = dx; dst.yAt(idx) = dy; }
        } else {
            const float ax = points.xAt(0), ay = points.yAt(0), bx = points.xAt(1), by = points.yAt(1);
            for (; idx + 4 <= samples.size(); idx += 4) {
                float xs[4], ys[4];
                for (std::size_t lane = 0; lane < 4; ++lane) {
                    const float t = samples[idx + lane], one = 1.0f - t;
                    xs[lane] = ax * one + bx * t; ys[lane] = ay * one + by * t;
                }
                for (std::size_t lane = 0; lane < 4; ++lane) dst.xAt(idx + lane) = xs[lane];
                for (std::size_t lane = 0; lane < 4; ++lane) dst.yAt(idx + lane) = ys[lane];
            }
            for (; idx < samples.size(); ++idx) {
                const float t = samples[idx];
                dst.xAt(idx) = points.xAt(0) * (1 - t) + points.xAt(1) * t;
                dst.yAt(idx) = points.yAt(0) * (1 - t) + points.yAt(1) * t;
            }
        }
        return;
    }
    const auto wrapAdd = [](std::int32_t value, std::int32_t delta) {
        return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(value) +
            std::bit_cast<std::uint32_t>(delta));
    };
    const auto evaluate = [&](float t, bool fourLane) {
        const float segment = t * static_cast<float>(len - 1);
        const auto segmentIndex = static_cast<std::int32_t>(segment);
        const auto signedLength = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(len));
        const auto upper = wrapAdd(signedLength, -2);
#ifndef NDEBUG
        if (upper < 0) throw d_runtime::ngStaticError<d_runtime::AssertError>(
            "Lower can't be greater than upper.", "std/algorithm/comparison.d", 588);
#endif
        const auto p1 = segmentIndex < 0 ? 0 : segmentIndex > upper ? upper : segmentIndex;
        const auto p0 = std::max(std::int32_t{0}, wrapAdd(p1, -1));
        const auto last = wrapAdd(signedLength, -1);
        const auto p2 = std::min(last, wrapAdd(p1, 1));
        const auto p3 = std::min(last, wrapAdd(p2, 1));
        const float lt = segment - segmentIndex, lt2 = lt * lt;
        const std::size_t firstLine = Derivative ? (fourLane ? 497 : 535) : (fourLane ? 375 : 418);
        const auto component = [&](std::int32_t position, bool y, std::size_t line) {
            const auto index = static_cast<std::size_t>(position);
            if (index >= points.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
                index, points.size(), d_runtime::RetainedString::fromStaticLiteral(
                    "source/nijilive/core/nodes/deformer/curve.d"), line);
            return y ? points.yAt(index) : points.xAt(index);
        };
        const float p0x = component(p0, false, firstLine), p0y = component(p0, true, firstLine + 1);
        const float p1x = component(p1, false, firstLine + 2), p1y = component(p1, true, firstLine + 3);
        const float p2x = component(p2, false, firstLine + 4), p2y = component(p2, true, firstLine + 5);
        const float p3x = component(p3, false, firstLine + 6), p3y = component(p3, true, firstLine + 7);
        float ax = 0, ay = 0;
        if constexpr (!Derivative) { ax = 2.0f * p1x; ay = 2.0f * p1y; }
        const float bx = p2x - p0x, by = p2y - p0y;
        const float cx = 2.0f * p0x - 5.0f * p1x + 4.0f * p2x - p3x;
        const float cy = 2.0f * p0y - 5.0f * p1y + 4.0f * p2y - p3y;
        const float dx = -p0x + 3.0f * p1x - 3.0f * p2x + p3x;
        const float dy = -p0y + 3.0f * p1y - 3.0f * p2y + p3y;
        if constexpr (Derivative) {
            return Vec2{0.5f * (bx + 2.0f * cx * lt + 3.0f * dx * lt2),
                        0.5f * (by + 2.0f * cy * lt + 3.0f * dy * lt2)};
        } else {
            const float lt3 = lt2 * lt;
            return Vec2{0.5f * (ax + bx * lt + cx * lt2 + dx * lt3),
                        0.5f * (ay + by * lt + cy * lt2 + dy * lt3)};
        }
    };
    for (; idx + 4 <= samples.size(); idx += 4) {
        Vec2 values[4];
        for (std::size_t lane = 0; lane < 4; ++lane) values[lane] = evaluate(samples[idx + lane], true);
        for (std::size_t lane = 0; lane < 4; ++lane) dst.xAt(idx + lane) = values[lane].x;
        for (std::size_t lane = 0; lane < 4; ++lane) dst.yAt(idx + lane) = values[lane].y;
    }
    for (; idx < samples.size(); ++idx) {
        const auto value = evaluate(samples[idx], false); dst.xAt(idx) = value.x; dst.yAt(idx) = value.y;
    }
}

// Adapted from Phobos std.numeric.findLocalMin, Copyright Andrei Alexandrescu
// 2008-2009. BSL-1.0; see licenses/druntime-string-hash.txt for the license text.
template<class Function> float sourceFindLocalMin01(Function&& function) {
    constexpr float c = 0x0.61c8864680b583ea0c633f9fa31237p+0L;
    constexpr float cm1 = 0x0.9e3779b97f4a7c15f39cc0605cedc8p+0L;
    const float relative = std::sqrt(std::numeric_limits<float>::epsilon());
    const float absolute = std::sqrt(std::numeric_limits<float>::epsilon());
#ifndef NDEBUG
    const auto require = [](bool condition, const char* message, unsigned line) {
        if (!condition) throw d_runtime::ngStaticError<d_runtime::AssertError>(message, "std/numeric.d", line);
    };
    require(std::isfinite(0.0f), "ax is not finite", 1786);
    require(std::isfinite(1.0f), "bx is not finite", 1787);
    require(std::isnormal(relative), "relTolerance is not normal floating point number", 1788);
    require(std::isnormal(absolute), "absTolerance is not normal floating point number", 1789);
    require(relative >= 0, "absTolerance is not positive", 1790);
    require(absolute >= std::numeric_limits<float>::epsilon() * 2,
        "absTolerance is not greater then `2*T.epsilon`", 1791);
#endif
    const auto finish = [](float value) {
#ifndef NDEBUG
        if (!std::isfinite(value)) throw d_runtime::ngStaticError<d_runtime::AssertError>(
            "Assertion failure", "std/numeric.d", 1795);
#endif
        return value;
    };
    float a = 0, b = 1;
    float v = a * cm1 + b * c;
#ifndef NDEBUG
    if (!std::isfinite(v)) throw d_runtime::ngStaticError<d_runtime::AssertError>(
        "Assertion failure", "std/numeric.d", 1810);
#endif
    // inmath Vector!(float,2).lengthSquared returns real; CommonType!(real,
    // float) keeps the target's native real width throughout the Brent state.
    long double fv = function(v);
    if (std::isnan(fv) || fv == -std::numeric_limits<float>::infinity()) return finish(v);
    float w = v, x = v;
    long double fw = fv, fx = fv;
    for (long double d = 0, e = 0;;) {
        float midpoint = (a + b) / 2;
        if (!std::isfinite(midpoint)) {
            midpoint = a / 2 + b / 2;
            if (!std::isfinite(midpoint)) {
                int aExponent, bExponent;
                const auto aNormal = std::frexp(a, &aExponent);
                const auto bNormal = std::frexp(b, &bExponent);
                const auto aMiddle = std::ldexp(aNormal, aExponent - 1);
                const auto bMiddle = std::ldexp(bNormal, bExponent - 1);
                midpoint = aMiddle + bMiddle;
                if (!std::isfinite(midpoint)) return finish(std::numeric_limits<float>::quiet_NaN());
            }
        }
        const long double tolerance = absolute * std::fabs(x) + relative;
        const auto twice = tolerance * 2;
        if (!(std::fabs(x - midpoint) > twice - (b - a) / 2)) break;
        long double p = 0, q = 0, r = 0;
        if (std::fabs(e) > tolerance) {
            const float xw = x - w, xv = x - v;
            const auto fxw = fx - fw, fxv = fx - fv;
            const auto xwfxv = xw * fxv, xvfxw = xv * fxw;
            p = xv * xvfxw - xw * xwfxv;
            q = (xvfxw - xwfxv) * 2;
            if (q > 0) p = -p;
            else q = -q;
            r = e;
            e = d;
        }
        float u;
        if (std::fabs(p) < std::fabs(q * r / 2) && p > q * (a - x) && p < q * (b - x)) {
            d = p / q;
            u = static_cast<float>(x + d);
            if (u - a < twice || b - u < twice) d = x < midpoint ? tolerance : -tolerance;
        } else {
            e = (x < midpoint ? b : a) - x;
            d = c * e;
        }
        u = static_cast<float>(x + (std::fabs(d) >= tolerance ? d : d > 0 ? tolerance : -tolerance));
        const auto fu = function(u);
        if (std::isnan(fu) || fu == -std::numeric_limits<float>::infinity()) return finish(u);
        if (fu <= fx) {
            (u < x ? b : a) = x;
            v = w; fv = fw;
            w = x; fw = fx;
            x = u; fx = fu;
        } else {
            (u < x ? a : b) = u;
            if (fu <= fw || w == x) {
                v = w; fv = fw;
                w = u; fw = fu;
            } else if (fu <= fv || v == x || v == w) {
                v = u; fv = fu;
            }
        }
    }
    return finish(x);
}

float findLocalMin01(const std::function<double(float)>& f) {
    // D std.numeric.findLocalMin (Brent) parity.
    constexpr float c = 0x0.61c8864680b583ea0c633f9fa31237p+0L; // (3-sqrt(5))/2
    constexpr float cm1 = 0x0.9e3779b97f4a7c15f39cc0605cedc8p+0L;
    const float relTolerance = std::sqrt(std::numeric_limits<float>::epsilon());
    const float absTolerance = std::sqrt(std::numeric_limits<float>::epsilon());

    float a = 0.0f;
    float b = 1.0f;
    float v = a * cm1 + b * c;
    double fv = f(v);
    if (std::isnan(fv) || fv == -std::numeric_limits<float>::infinity()) {
        return v;
    }
    float w = v;
    double fw = fv;
    float x = v;
    double fx = fv;
    double d = 0.0f;
    double e = 0.0f;

    for (;;) {
        float m = (a + b) * 0.5f;
        if (!std::isfinite(m)) {
            m = a * 0.5f + b * 0.5f;
            if (!std::isfinite(m)) return std::numeric_limits<float>::quiet_NaN();
        }
        double tolerance = absTolerance * std::fabs(x) + relTolerance;
        double t2 = tolerance * 2.0f;
        if (!(std::fabs(x - m) > t2 - (b - a) * 0.5f)) break;

        double p = 0.0f;
        double q = 0.0f;
        double r = 0.0f;
        if (std::fabs(e) > tolerance) {
            const float xw = x - w;
            const double fxw = fx - fw;
            const float xv = x - v;
            const double fxv = fx - fv;
            const double xwfxv = xw * fxv;
            const double xvfxw = xv * fxw;
            p = xv * xvfxw - xw * xwfxv;
            q = (xvfxw - xwfxv) * 2.0f;
            if (q > 0.0f) p = -p; else q = -q;
            r = e;
            e = d;
        }

        float u = 0.0f;
        if (std::fabs(p) < std::fabs(q * r * 0.5f) && p > q * (a - x) && p < q * (b - x)) {
            d = p / q;
            u = static_cast<float>(x + d);
            if (u - a < t2 || b - u < t2) {
                d = x < m ? tolerance : -tolerance;
            }
        } else {
            e = (x < m ? b : a) - x;
            d = c * e;
        }

        u = static_cast<float>(x + (std::fabs(d) >= tolerance ? d : (d > 0.0f ? tolerance : -tolerance)));
        const double fu = f(u);
        if (std::isnan(fu) || fu == -std::numeric_limits<float>::infinity()) {
            return u;
        }

        if (fu <= fx) {
            if (u < x) b = x; else a = x;
            v = w; fv = fw;
            w = x; fw = fx;
            x = u; fx = fu;
        } else {
            if (u < x) a = u; else b = u;
            if (fu <= fw || w == x) {
                v = w; fv = fw;
                w = u; fw = fu;
            } else if (fu <= fv || v == x || v == w) {
                v = u; fv = fu;
            }
        }
    }
    return x;
}
} // namespace

float BezierCurve::binomial(int n, int k) {
    if (k > n) return 0.0f;
    if (k == 0 || k == n) return 1.0f;
    k = std::min(k, n - k);
    float res = 1.0f;
    for (int i = 0; i < k; ++i) {
        res *= static_cast<float>(n - i) / static_cast<float>(i + 1);
    }
    return res;
}

void BezierCurve::evaluatePointsSource(const common::SharedArray<float> samples, Vec2Array& dst) {
    sourceBezierEvaluate(controlPoints_, samples, dst);
}
void BezierCurve::evaluateDerivativesSource(const common::SharedArray<float> samples, Vec2Array& dst) {
    sourceBezierEvaluate(derivatives_, samples, dst);
}
void SplineCurve::evaluatePointsSource(const common::SharedArray<float> samples, Vec2Array& dst) {
    sourceSplineEvaluate<false>(controlPoints_, samples, dst);
}
void SplineCurve::evaluateDerivativesSource(const common::SharedArray<float> samples, Vec2Array& dst) {
    sourceSplineEvaluate<true>(controlPoints_, samples, dst);
}

BezierCurve::BezierCurve(const Vec2Array& pts) : controlPoints_(pts.dup()) {
    recomputeDerivativesForSourceConstruction();
}

void BezierCurve::initializeSource(Vec2Array points) {
    auto duplicate = points.dup();
    setControlPointsSource(duplicate);
    const auto count = points.size();
    derivatives_ = Vec2Array(count ? count - 1 : 0);
    calculateDerivatives();
}

Vec2 BezierCurve::pointSource(float t) {
    if (pointCache.contains(t)) return pointCache.at(t);
    Vec2Array temporary;
    common::SharedArray<float> samples(1, t);
    evaluatePointsSource(samples, temporary);
    const Vec2 result = sourceCurveFirst(temporary);
    pointCache[t] = result;
    return result;
}

Vec2 BezierCurve::derivativeSource(float t) {
    Vec2Array temporary;
    common::SharedArray<float> samples(1, t);
    evaluateDerivativesSource(samples, temporary);
    return sourceCurveFirst(temporary);
}

void BezierCurve::setControlPointsSource(Vec2Array& points) {
    // The original setter assigns only the field; it neither regenerates
    // derivatives nor clears cached points.
    controlPoints_ = points;
}

float BezierCurve::closestPointSource(Vec2 point, int samples) {
    (void)samples;
    return sourceFindLocalMin01([&](float t) -> long double {
        const auto evaluated = pointSource(t);
        const float x = evaluated.x - point.x, y = evaluated.y - point.y;
        long double squared = 0;
        squared += x * x;
        squared += y * y;
        return squared;
    });
}

Vec2 SplineCurve::pointSource(float t) {
    if (pointCache.contains(t)) return pointCache.at(t);
    Vec2Array temporary;
    common::SharedArray<float> samples(1, t);
    evaluatePointsSource(samples, temporary);
    const Vec2 result = temporary.empty() ? Vec2{0, 0} : sourceCurveFirst(temporary);
    pointCache[t] = result;
    return result;
}

Vec2 SplineCurve::derivativeSource(float t) {
    if (derivativeCache.contains(t)) return derivativeCache.at(t);
    Vec2Array temporary;
    common::SharedArray<float> samples(1, t);
    evaluateDerivativesSource(samples, temporary);
    const Vec2 result = temporary.empty() ? Vec2{0, 0} : sourceCurveFirst(temporary);
    derivativeCache[t] = result;
    return result;
}

void SplineCurve::setControlPointsSource(Vec2Array& points) {
    controlPoints_ = points;
    pointCache.clear();
    derivativeCache.clear();
}

float SplineCurve::closestPointSource(Vec2 point, int samples) {
    (void)samples;
    return sourceFindLocalMin01([&](float t) -> long double {
        const auto evaluated = pointSource(t);
        const float x = evaluated.x - point.x, y = evaluated.y - point.y;
        long double squared = 0;
        squared += x * x;
        squared += y * y;
        return squared;
    });
}

void BezierCurve::setControlPoints(const Vec2Array& pts) {
    controlPoints_ = pts;
    recomputeDerivatives();
}

void BezierCurve::recomputeDerivatives() {
    auto n = controlPoints_.size();
    derivatives_.resize(n > 0 ? n - 1 : 0);
    if (n == 0) return;
    int order = static_cast<int>(n) - 1;
    for (int i = 0; i < order; ++i) {
        derivatives_.xAt(i) = (controlPoints_.xAt(i + 1) - controlPoints_.xAt(i)) * order;
        derivatives_.yAt(i) = (controlPoints_.yAt(i + 1) - controlPoints_.yAt(i)) * order;
    }
}

void BezierCurve::recomputeDerivativesForSourceConstruction() {
    const auto count = controlPoints_.size();
    derivatives_.resize(count ? count - 1 : 0);
    calculateDerivatives();
}

void BezierCurve::calculateDerivatives() {
    const auto order = std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(controlPoints().size()) - 1);
    const auto point = [&](std::int32_t position) {
        const auto index = static_cast<std::size_t>(position);
        const auto& values = controlPoints();
#ifndef NDEBUG
        if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::AssertError>(
            "veca index out of range", "source/nijilive/math/veca.d", 139);
#endif
        if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
            index, values.size(), d_runtime::RetainedString::fromStaticLiteral("source/nijilive/math/veca.d"), 467);
        return Vec2{values.xAt(index), values.yAt(index)};
    };
    const auto next = [](std::int32_t index) {
        return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(index) + std::uint32_t{1});
    };
    for (std::int32_t i = 0; static_cast<std::int64_t>(i) < order; i = next(i)) {
        const auto index = static_cast<std::size_t>(i);
        // Source evaluates the destination index before the right-hand side.
#ifndef NDEBUG
        if (index >= derivatives_.size()) throw d_runtime::ngStaticError<d_runtime::AssertError>(
            "veca index out of range", "source/nijilive/math/veca.d", 139);
#endif
        const auto right = point(next(i));
        const auto left = point(i);
        const float factor = static_cast<float>(order);
        const Vec2 result{(right.x - left.x) * factor, (right.y - left.y) * factor};
        if (index >= derivatives_.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
            index, derivatives_.size(), d_runtime::RetainedString::fromStaticLiteral("source/nijilive/math/veca.d"), 475);
        derivatives_.xAt(index) = result.x;
        derivatives_.yAt(index) = result.y;
    }
}

Vec2 BezierCurve::point(float t) const {
    std::vector<float> ts{t};
    Vec2Array dst;
    evaluatePoints(ts, dst);
    if (dst.size() == 0) return {};
    return Vec2{dst.xAt(0), dst.yAt(0)};
}

Vec2 BezierCurve::derivative(float t) const {
    std::vector<float> ts{t};
    Vec2Array dst;
    evaluateDerivatives(ts, dst);
    if (dst.size() == 0) return {};
    return Vec2{dst.xAt(0), dst.yAt(0)};
}

void BezierCurve::evaluatePoints(const std::vector<float>& tSamples, Vec2Array& dst) const {
    dst.resize(tSamples.size());
    auto n = static_cast<int>(controlPoints_.size());
    if (tSamples.empty() || n == 0) return;
    int order = n - 1;
    std::vector<float> binomials(order + 1, 0.0f);
    std::vector<float> tPow(order + 1, 1.0f);
    std::vector<float> onePow(order + 1, 1.0f);
    for (int i = 0; i <= order; ++i) {
        binomials[i] = binomial(order, i);
    }
    for (std::size_t idx = 0; idx < tSamples.size(); ++idx) {
        float t = tSamples[idx];
        float oneMinus = 1.0f - t;
        float resX = 0.0f, resY = 0.0f;
        tPow[0] = 1.0f;
        onePow[0] = 1.0f;
        for (int i = 1; i <= order; ++i) {
            tPow[i] = tPow[i - 1] * t;
            onePow[i] = onePow[i - 1] * oneMinus;
        }
        for (int i = 0; i <= order; ++i) {
            float coeff = binomials[i] * onePow[order - i] * tPow[i];
            resX += coeff * controlPoints_.xAt(i);
            resY += coeff * controlPoints_.yAt(i);
        }
        dst.xAt(idx) = resX;
        dst.yAt(idx) = resY;
    }
}

void BezierCurve::evaluateDerivatives(const std::vector<float>& tSamples, Vec2Array& dst) const {
    dst.resize(tSamples.size());
    auto n = static_cast<int>(derivatives_.size());
    if (tSamples.empty() || n == 0) return;
    int order = n - 1;
    std::vector<float> binomials(order + 1, 0.0f);
    std::vector<float> tPow(order + 1, 1.0f);
    std::vector<float> onePow(order + 1, 1.0f);
    for (int i = 0; i <= order; ++i) {
        binomials[i] = binomial(order, i);
    }
    for (std::size_t idx = 0; idx < tSamples.size(); ++idx) {
        float t = tSamples[idx];
        float oneMinus = 1.0f - t;
        float resX = 0.0f, resY = 0.0f;
        tPow[0] = 1.0f;
        onePow[0] = 1.0f;
        for (int i = 1; i <= order; ++i) {
            tPow[i] = tPow[i - 1] * t;
            onePow[i] = onePow[i - 1] * oneMinus;
        }
        for (int i = 0; i <= order; ++i) {
            float coeff = binomials[i] * onePow[order - i] * tPow[i];
            resX += coeff * derivatives_.xAt(i);
            resY += coeff * derivatives_.yAt(i);
        }
        dst.xAt(idx) = resX;
        dst.yAt(idx) = resY;
    }
}

float BezierCurve::closestPoint(const Vec2& p, int samples) const {
    (void)samples;
    return findLocalMin01([&](float t) {
        const Vec2 pt = point(t);
        const float dx = pt.x - p.x;
        const float dy = pt.y - p.y;
        return static_cast<double>(dx * dx) + dy * dy;
    });
}

SplineCurve::SplineCurve(const Vec2Array& pts) : controlPoints_(pts.dup()) {}

void SplineCurve::initializeSource(Vec2Array points) {
    auto duplicate = points.dup();
    setControlPointsSource(duplicate);
    pointCache.clear();
    derivativeCache.clear();
}

void SplineCurve::setControlPoints(const Vec2Array& pts) {
    controlPoints_ = pts;
}

Vec2 SplineCurve::point(float t) const {
    std::vector<float> ts{t};
    Vec2Array dst;
    evaluatePoints(ts, dst);
    if (dst.size() == 0) return {};
    return Vec2{dst.xAt(0), dst.yAt(0)};
}

Vec2 SplineCurve::derivative(float t) const {
    std::vector<float> ts{t};
    Vec2Array dst;
    evaluateDerivatives(ts, dst);
    if (dst.size() == 0) return {};
    return Vec2{dst.xAt(0), dst.yAt(0)};
}

void SplineCurve::evaluatePoints(const std::vector<float>& tSamples, Vec2Array& dst) const {
    dst.resize(tSamples.size());
    std::size_t len = controlPoints_.size();
    if (tSamples.empty() || len < 2) {
        dst.fill(Vec2{0.0f, 0.0f});
        return;
    }
    if (len == 2) {
        const float ax = controlPoints_.xAt(0);
        const float ay = controlPoints_.yAt(0);
        const float bx = controlPoints_.xAt(1);
        const float by = controlPoints_.yAt(1);
        for (std::size_t idx = 0; idx < tSamples.size(); ++idx) {
            const float t = tSamples[idx];
            dst.xAt(idx) = ax * (1.0f - t) + bx * t;
            dst.yAt(idx) = ay * (1.0f - t) + by * t;
        }
        return;
    }
    for (std::size_t idx = 0; idx < tSamples.size(); ++idx) {
        float t = tSamples[idx];
        float segment = t * static_cast<float>(len - 1);
        int segmentIndex = static_cast<int>(segment);
        int p1 = std::clamp(segmentIndex, 0, static_cast<int>(len) - 2);
        int p0 = std::max(0, p1 - 1);
        int p2 = std::min(static_cast<int>(len) - 1, p1 + 1);
        int p3 = std::min(static_cast<int>(len) - 1, p2 + 1);
        float lt = segment - static_cast<float>(segmentIndex);
        float lt2 = lt * lt;
        float lt3 = lt2 * lt;
        float ax = 2.0f * controlPoints_.xAt(p1);
        float ay = 2.0f * controlPoints_.yAt(p1);
        float bx = controlPoints_.xAt(p2) - controlPoints_.xAt(p0);
        float by = controlPoints_.yAt(p2) - controlPoints_.yAt(p0);
        float cx = 2.0f * controlPoints_.xAt(p0) - 5.0f * controlPoints_.xAt(p1) + 4.0f * controlPoints_.xAt(p2) - controlPoints_.xAt(p3);
        float cy = 2.0f * controlPoints_.yAt(p0) - 5.0f * controlPoints_.yAt(p1) + 4.0f * controlPoints_.yAt(p2) - controlPoints_.yAt(p3);
        float dx = -controlPoints_.xAt(p0) + 3.0f * controlPoints_.xAt(p1) - 3.0f * controlPoints_.xAt(p2) + controlPoints_.xAt(p3);
        float dy = -controlPoints_.yAt(p0) + 3.0f * controlPoints_.yAt(p1) - 3.0f * controlPoints_.yAt(p2) + controlPoints_.yAt(p3);
        dst.xAt(idx) = 0.5f * (ax + bx * lt + cx * lt2 + dx * lt3);
        dst.yAt(idx) = 0.5f * (ay + by * lt + cy * lt2 + dy * lt3);
    }
}

void SplineCurve::evaluateDerivatives(const std::vector<float>& tSamples, Vec2Array& dst) const {
    dst.resize(tSamples.size());
    std::size_t len = controlPoints_.size();
    if (tSamples.empty() || len < 2) {
        dst.fill(Vec2{0.0f, 0.0f});
        return;
    }
    if (len == 2) {
        const float dx = controlPoints_.xAt(1) - controlPoints_.xAt(0);
        const float dy = controlPoints_.yAt(1) - controlPoints_.yAt(0);
        for (std::size_t idx = 0; idx < tSamples.size(); ++idx) {
            dst.xAt(idx) = dx;
            dst.yAt(idx) = dy;
        }
        return;
    }
    for (std::size_t idx = 0; idx < tSamples.size(); ++idx) {
        float t = tSamples[idx];
        float segment = t * static_cast<float>(len - 1);
        int segmentIndex = static_cast<int>(segment);
        int p1 = std::clamp(segmentIndex, 0, static_cast<int>(len) - 2);
        int p0 = std::max(0, p1 - 1);
        int p2 = std::min(static_cast<int>(len) - 1, p1 + 1);
        int p3 = std::min(static_cast<int>(len) - 1, p2 + 1);
        float lt = segment - static_cast<float>(segmentIndex);
        float lt2 = lt * lt;
        float ax = 2.0f * controlPoints_.xAt(p1);
        float ay = 2.0f * controlPoints_.yAt(p1);
        float bx = controlPoints_.xAt(p2) - controlPoints_.xAt(p0);
        float by = controlPoints_.yAt(p2) - controlPoints_.yAt(p0);
        float cx = 2.0f * controlPoints_.xAt(p0) - 5.0f * controlPoints_.xAt(p1) + 4.0f * controlPoints_.xAt(p2) - controlPoints_.xAt(p3);
        float cy = 2.0f * controlPoints_.yAt(p0) - 5.0f * controlPoints_.yAt(p1) + 4.0f * controlPoints_.yAt(p2) - controlPoints_.yAt(p3);
        float dx = -controlPoints_.xAt(p0) + 3.0f * controlPoints_.xAt(p1) - 3.0f * controlPoints_.xAt(p2) + controlPoints_.xAt(p3);
        float dy = -controlPoints_.yAt(p0) + 3.0f * controlPoints_.yAt(p1) - 3.0f * controlPoints_.yAt(p2) + controlPoints_.yAt(p3);
        dst.xAt(idx) = 0.5f * (bx + 2.0f * cx * lt + 3.0f * dx * lt2);
        dst.yAt(idx) = 0.5f * (by + 2.0f * cy * lt + 3.0f * dy * lt2);
    }
}

float SplineCurve::closestPoint(const Vec2& p, int samples) const {
    (void)samples;
    return findLocalMin01([&](float t) {
        const Vec2 pt = point(t);
        const float dx = pt.x - p.x;
        const float dy = pt.y - p.y;
        return static_cast<double>(dx * dx) + dy * dy;
    });
}

std::unique_ptr<Curve> createCurve(const Vec2Array& pts, bool bezier) {
    if (bezier) {
        return std::make_unique<BezierCurve>(pts);
    }
    return std::make_unique<SplineCurve>(pts);
}

} // namespace nicxlive::core::nodes
