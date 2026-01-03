#pragma once

#include "../common/utils.hpp"
#include "common.hpp"

#include <vector>
#include <memory>

namespace nicxlive::core::nodes {

class Curve {
public:
    virtual ~Curve() = default;
    virtual Vec2 point(float t) const = 0;
    virtual float closestPoint(const Vec2& p, int samples = 100) const = 0;
    virtual core::common::Vec2Array& controlPoints() = 0;
    virtual void setControlPoints(const core::common::Vec2Array& pts) = 0;
    virtual Vec2 derivative(float t) const = 0;
    virtual void evaluatePoints(const std::vector<float>& tSamples, core::common::Vec2Array& dst) const = 0;
    virtual void evaluateDerivatives(const std::vector<float>& tSamples, core::common::Vec2Array& dst) const = 0;
};

class BezierCurve : public Curve {
public:
    explicit BezierCurve(const core::common::Vec2Array& pts);

    Vec2 point(float t) const override;
    float closestPoint(const Vec2& p, int samples = 100) const override;
    core::common::Vec2Array& controlPoints() override { return controlPoints_; }
    void setControlPoints(const core::common::Vec2Array& pts) override;
    Vec2 derivative(float t) const override;
    void evaluatePoints(const std::vector<float>& tSamples, core::common::Vec2Array& dst) const override;
    void evaluateDerivatives(const std::vector<float>& tSamples, core::common::Vec2Array& dst) const override;

private:
    core::common::Vec2Array controlPoints_{};
    core::common::Vec2Array derivatives_{};
    static float binomial(int n, int k);
    void recomputeDerivatives();
};

class SplineCurve : public Curve {
public:
    explicit SplineCurve(const core::common::Vec2Array& pts);

    Vec2 point(float t) const override;
    float closestPoint(const Vec2& p, int samples = 100) const override;
    core::common::Vec2Array& controlPoints() override { return controlPoints_; }
    void setControlPoints(const core::common::Vec2Array& pts) override;
    Vec2 derivative(float t) const override;
    void evaluatePoints(const std::vector<float>& tSamples, core::common::Vec2Array& dst) const override;
    void evaluateDerivatives(const std::vector<float>& tSamples, core::common::Vec2Array& dst) const override;

private:
    core::common::Vec2Array controlPoints_{};
};

std::unique_ptr<Curve> createCurve(const core::common::Vec2Array& pts, bool bezier);

} // namespace nicxlive::core::nodes
