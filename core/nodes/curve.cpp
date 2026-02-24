#include "curve.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace nicxlive::core::nodes {

using ::nicxlive::core::common::Vec2Array;

namespace {
float findLocalMin01(const std::function<float(float)>& f) {
    // D std.numeric.findLocalMin (Brent) parity.
    constexpr float c = 0x0.61c8864680b583ea0c633f9fa31237p+0L; // (3-sqrt(5))/2
    constexpr float cm1 = 0x0.9e3779b97f4a7c15f39cc0605cedc8p+0L;
    const float relTolerance = std::sqrt(std::numeric_limits<float>::epsilon());
    const float absTolerance = std::sqrt(std::numeric_limits<float>::epsilon());

    float a = 0.0f;
    float b = 1.0f;
    float v = a * cm1 + b * c;
    float fv = f(v);
    if (!std::isfinite(fv) || fv == -std::numeric_limits<float>::infinity()) {
        return std::clamp(v, 0.0f, 1.0f);
    }
    float w = v;
    float fw = fv;
    float x = v;
    float fx = fv;
    float d = 0.0f;
    float e = 0.0f;

    for (;;) {
        float m = (a + b) * 0.5f;
        if (!std::isfinite(m)) {
            m = a * 0.5f + b * 0.5f;
            if (!std::isfinite(m)) return std::clamp(x, 0.0f, 1.0f);
        }
        float tolerance = absTolerance * std::fabs(x) + relTolerance;
        float t2 = tolerance * 2.0f;
        if (!(std::fabs(x - m) > t2 - (b - a) * 0.5f)) break;

        float p = 0.0f;
        float q = 0.0f;
        float r = 0.0f;
        if (std::fabs(e) > tolerance) {
            const float xw = x - w;
            const float fxw = fx - fw;
            const float xv = x - v;
            const float fxv = fx - fv;
            const float xwfxv = xw * fxv;
            const float xvfxw = xv * fxw;
            p = xv * xvfxw - xw * xwfxv;
            q = (xvfxw - xwfxv) * 2.0f;
            if (q > 0.0f) p = -p; else q = -q;
            r = e;
            e = d;
        }

        float u = 0.0f;
        if (std::fabs(p) < std::fabs(q * r * 0.5f) && p > q * (a - x) && p < q * (b - x)) {
            d = p / q;
            u = x + d;
            if (u - a < t2 || b - u < t2) {
                d = x < m ? tolerance : -tolerance;
            }
        } else {
            e = (x < m ? b : a) - x;
            d = c * e;
        }

        u = x + (std::fabs(d) >= tolerance ? d : (d > 0.0f ? tolerance : -tolerance));
        const float fu = f(u);
        if (!std::isfinite(fu) || fu == -std::numeric_limits<float>::infinity()) {
            return std::clamp(u, 0.0f, 1.0f);
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
    return std::clamp(x, 0.0f, 1.0f);
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

BezierCurve::BezierCurve(const Vec2Array& pts) : controlPoints_(pts) {
    recomputeDerivatives();
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
    for (std::size_t idx = 0; idx < tSamples.size(); ++idx) {
        float t = std::clamp(tSamples[idx], 0.0f, 1.0f);
        float oneMinus = 1.0f - t;
        float resX = 0.0f, resY = 0.0f;
        std::vector<float> tPow(order + 1, 1.0f);
        std::vector<float> onePow(order + 1, 1.0f);
        for (int i = 1; i <= order; ++i) {
            tPow[i] = tPow[i - 1] * t;
            onePow[i] = onePow[i - 1] * oneMinus;
        }
        for (int i = 0; i <= order; ++i) {
            float coeff = binomial(order, i) * onePow[order - i] * tPow[i];
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
    for (std::size_t idx = 0; idx < tSamples.size(); ++idx) {
        float t = std::clamp(tSamples[idx], 0.0f, 1.0f);
        float oneMinus = 1.0f - t;
        float resX = 0.0f, resY = 0.0f;
        std::vector<float> tPow(order + 1, 1.0f);
        std::vector<float> onePow(order + 1, 1.0f);
        for (int i = 1; i <= order; ++i) {
            tPow[i] = tPow[i - 1] * t;
            onePow[i] = onePow[i - 1] * oneMinus;
        }
        for (int i = 0; i <= order; ++i) {
            float coeff = binomial(order, i) * onePow[order - i] * tPow[i];
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
        return dx * dx + dy * dy;
    });
}

SplineCurve::SplineCurve(const Vec2Array& pts) : controlPoints_(pts) {}

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
            const float t = std::clamp(tSamples[idx], 0.0f, 1.0f);
            dst.xAt(idx) = ax * (1.0f - t) + bx * t;
            dst.yAt(idx) = ay * (1.0f - t) + by * t;
        }
        return;
    }
    for (std::size_t idx = 0; idx < tSamples.size(); ++idx) {
        float t = std::clamp(tSamples[idx], 0.0f, 1.0f);
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
        float t = std::clamp(tSamples[idx], 0.0f, 1.0f);
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
        return dx * dx + dy * dy;
    });
}

std::unique_ptr<Curve> createCurve(const Vec2Array& pts, bool bezier) {
    if (bezier) {
        return std::make_unique<BezierCurve>(pts);
    }
    return std::make_unique<SplineCurve>(pts);
}

} // namespace nicxlive::core::nodes
