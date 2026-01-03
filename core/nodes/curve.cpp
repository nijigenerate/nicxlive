#include "curve.hpp"

#include <algorithm>
#include <cmath>

namespace nicxlive::core::nodes {

using ::nicxlive::core::common::Vec2Array;

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
        derivatives_.x[i] = (controlPoints_.x[i + 1] - controlPoints_.x[i]) * order;
        derivatives_.y[i] = (controlPoints_.y[i + 1] - controlPoints_.y[i]) * order;
    }
}

Vec2 BezierCurve::point(float t) const {
    std::vector<float> ts{t};
    Vec2Array dst;
    evaluatePoints(ts, dst);
    if (dst.size() == 0) return {};
    return Vec2{dst.x[0], dst.y[0]};
}

Vec2 BezierCurve::derivative(float t) const {
    std::vector<float> ts{t};
    Vec2Array dst;
    evaluateDerivatives(ts, dst);
    if (dst.size() == 0) return {};
    return Vec2{dst.x[0], dst.y[0]};
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
            resX += coeff * controlPoints_.x[i];
            resY += coeff * controlPoints_.y[i];
        }
        dst.x[idx] = resX;
        dst.y[idx] = resY;
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
            resX += coeff * derivatives_.x[i];
            resY += coeff * derivatives_.y[i];
        }
        dst.x[idx] = resX;
        dst.y[idx] = resY;
    }
}

float BezierCurve::closestPoint(const Vec2& p, int samples) const {
    float bestT = 0.0f;
    float bestDist = std::numeric_limits<float>::max();
    for (int i = 0; i <= samples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(std::max(1, samples));
        Vec2 pt = point(t);
        float dx = pt.x - p.x;
        float dy = pt.y - p.y;
        float d = dx * dx + dy * dy;
        if (d < bestDist) { bestDist = d; bestT = t; }
    }
    return bestT;
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
    return Vec2{dst.x[0], dst.y[0]};
}

Vec2 SplineCurve::derivative(float t) const {
    std::vector<float> ts{t};
    Vec2Array dst;
    evaluateDerivatives(ts, dst);
    if (dst.size() == 0) return {};
    return Vec2{dst.x[0], dst.y[0]};
}

void SplineCurve::evaluatePoints(const std::vector<float>& tSamples, Vec2Array& dst) const {
    dst.resize(tSamples.size());
    std::size_t len = controlPoints_.size();
    if (tSamples.empty() || len < 2) {
        std::fill(dst.x.begin(), dst.x.end(), 0.0f);
        std::fill(dst.y.begin(), dst.y.end(), 0.0f);
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
        float ax = 2.0f * controlPoints_.x[p1];
        float ay = 2.0f * controlPoints_.y[p1];
        float bx = controlPoints_.x[p2] - controlPoints_.x[p0];
        float by = controlPoints_.y[p2] - controlPoints_.y[p0];
        float cx = 2.0f * controlPoints_.x[p0] - 5.0f * controlPoints_.x[p1] + 4.0f * controlPoints_.x[p2] - controlPoints_.x[p3];
        float cy = 2.0f * controlPoints_.y[p0] - 5.0f * controlPoints_.y[p1] + 4.0f * controlPoints_.y[p2] - controlPoints_.y[p3];
        float dx = -controlPoints_.x[p0] + 3.0f * controlPoints_.x[p1] - 3.0f * controlPoints_.x[p2] + controlPoints_.x[p3];
        float dy = -controlPoints_.y[p0] + 3.0f * controlPoints_.y[p1] - 3.0f * controlPoints_.y[p2] + controlPoints_.y[p3];
        dst.x[idx] = 0.5f * (ax + bx * lt + cx * lt2 + dx * lt3);
        dst.y[idx] = 0.5f * (ay + by * lt + cy * lt2 + dy * lt3);
    }
}

void SplineCurve::evaluateDerivatives(const std::vector<float>& tSamples, Vec2Array& dst) const {
    dst.resize(tSamples.size());
    std::size_t len = controlPoints_.size();
    if (tSamples.empty() || len < 2) {
        std::fill(dst.x.begin(), dst.x.end(), 0.0f);
        std::fill(dst.y.begin(), dst.y.end(), 0.0f);
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
        float ax = 2.0f * controlPoints_.x[p1];
        float ay = 2.0f * controlPoints_.y[p1];
        float bx = controlPoints_.x[p2] - controlPoints_.x[p0];
        float by = controlPoints_.y[p2] - controlPoints_.y[p0];
        float cx = 2.0f * controlPoints_.x[p0] - 5.0f * controlPoints_.x[p1] + 4.0f * controlPoints_.x[p2] - controlPoints_.x[p3];
        float cy = 2.0f * controlPoints_.y[p0] - 5.0f * controlPoints_.y[p1] + 4.0f * controlPoints_.y[p2] - controlPoints_.y[p3];
        float dx = -controlPoints_.x[p0] + 3.0f * controlPoints_.x[p1] - 3.0f * controlPoints_.x[p2] + controlPoints_.x[p3];
        float dy = -controlPoints_.y[p0] + 3.0f * controlPoints_.y[p1] - 3.0f * controlPoints_.y[p2] + controlPoints_.y[p3];
        dst.x[idx] = 0.5f * (bx + 2.0f * cx * lt + 3.0f * dx * lt2);
        dst.y[idx] = 0.5f * (by + 2.0f * cy * lt + 3.0f * dy * lt2);
    }
}

float SplineCurve::closestPoint(const Vec2& p, int samples) const {
    float bestT = 0.0f;
    float bestDist = std::numeric_limits<float>::max();
    for (int i = 0; i <= samples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(std::max(1, samples));
        Vec2 pt = point(t);
        float dx = pt.x - p.x;
        float dy = pt.y - p.y;
        float d = dx * dx + dy * dy;
        if (d < bestDist) { bestDist = d; bestT = t; }
    }
    return bestT;
}

std::unique_ptr<Curve> createCurve(const Vec2Array& pts, bool bezier) {
    if (bezier) {
        return std::make_unique<BezierCurve>(pts);
    }
    return std::make_unique<SplineCurve>(pts);
}

} // namespace nicxlive::core::nodes
