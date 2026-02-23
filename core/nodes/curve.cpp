#include "curve.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

namespace nicxlive::core::nodes {

using ::nicxlive::core::common::Vec2Array;

namespace {
float findLocalMin01(const std::function<float(float)>& f) {
    // D std.numeric.findLocalMin parity intent:
    // detect local basins on a coarse scan, refine each with golden-section,
    // then choose the best refined minimum.
    constexpr int kSamples = 128;
    constexpr int kRefineIters = 32;
    constexpr float kInvPhi = 0.6180339887498948f; // (sqrt(5)-1)/2

    std::array<float, kSamples + 1> ts{};
    std::array<float, kSamples + 1> vs{};
    for (int i = 0; i <= kSamples; ++i) {
        ts[i] = static_cast<float>(i) / static_cast<float>(kSamples);
        vs[i] = f(ts[i]);
    }
    const float span = 1.0f / static_cast<float>(kSamples);
    float bestT = ts[0];
    float bestV = vs[0];

    auto refine = [&](float centerT) {
        float a = std::max(0.0f, centerT - span);
        float b = std::min(1.0f, centerT + span);
        float c = b - (b - a) * kInvPhi;
        float d = a + (b - a) * kInvPhi;
        float fc = f(c);
        float fd = f(d);
        for (int it = 0; it < kRefineIters; ++it) {
            if (fc < fd) {
                b = d;
                d = c;
                fd = fc;
                c = b - (b - a) * kInvPhi;
                fc = f(c);
            } else {
                a = c;
                c = d;
                fc = fd;
                d = a + (b - a) * kInvPhi;
                fd = f(d);
            }
        }
        const float t = std::clamp((a + b) * 0.5f, 0.0f, 1.0f);
        const float v = f(t);
        if (v < bestV) {
            bestV = v;
            bestT = t;
        }
    };

    refine(0.0f);
    for (int i = 1; i < kSamples; ++i) {
        if (vs[i] <= vs[i - 1] && vs[i] <= vs[i + 1]) {
            refine(ts[i]);
        }
    }
    refine(1.0f);
    return bestT;
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
