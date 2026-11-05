#pragma once

#include "../common/utils.hpp"
#include "common.hpp"
#include "../common/shared_array.hpp"
#include "../common/resource_key_map.hpp"
#include "../common/float_key_policy.hpp"

#include <vector>
#include <memory>
#include <type_traits>
#include <utility>

namespace nicxlive::core::nodes {

// Source factories first finish the actual object's storage, then run the
// original base constructor body once with complete-object virtual dispatch.
struct SourceCurveConstructionTag {};

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

// Public Source value-descriptor evaluation capability on the actual curve.
// Native Curve implementations retain their existing interface unchanged.
class SourceCurveEvaluation : public Curve {
public:
    virtual void evaluatePointsSource(common::SharedArray<float> samples, Vec2Array& dst) = 0;
    virtual void evaluateDerivativesSource(common::SharedArray<float> samples, Vec2Array& dst) = 0;
    virtual Vec2 pointSource(float t) = 0;
    virtual float closestPointSource(Vec2 point, int samples = 100) = 0;
    virtual void setControlPointsSource(core::common::Vec2Array& points) = 0;
    virtual Vec2 derivativeSource(float t) = 0;
};

class BezierCurve : public SourceCurveEvaluation {
public:
    explicit BezierCurve(const core::common::Vec2Array& pts);
    explicit BezierCurve(SourceCurveConstructionTag) {}
    void initializeSource(Vec2Array points);

    Vec2 point(float t) const override;
    float closestPoint(const Vec2& p, int samples = 100) const override;
    core::common::Vec2Array& controlPoints() override { return controlPoints_; }
    void setControlPoints(const core::common::Vec2Array& pts) override;
    Vec2 derivative(float t) const override;
    void evaluatePoints(const std::vector<float>& tSamples, core::common::Vec2Array& dst) const override;
    void evaluateDerivatives(const std::vector<float>& tSamples, core::common::Vec2Array& dst) const override;

    void evaluatePointsSource(common::SharedArray<float> samples, Vec2Array& dst) override;
    void evaluateDerivativesSource(common::SharedArray<float> samples, Vec2Array& dst) override;

    Vec2 pointSource(float t) override;
    float closestPointSource(Vec2 point, int samples = 100) override;
    void setControlPointsSource(Vec2Array& points) override;
    Vec2 derivativeSource(float t) override;
    using PointCache = common::PolicyKeyMap<float, Vec2, common::Float32KeyPolicy>;
    PointCache pointCache;

    // The original class exposes these actual descriptor fields publicly.
    core::common::Vec2Array controlPoints_{};
    core::common::Vec2Array derivatives_{};

    // Appended after existing virtual slots; does not resize or clear caches.
    virtual void calculateDerivatives();
    explicit operator bool() { return controlPoints().size() > 0; }

private:
    static float binomial(int n, int k);
    void recomputeDerivatives();
    void recomputeDerivativesForSourceConstruction();
};

class SplineCurve : public SourceCurveEvaluation {
public:
    explicit SplineCurve(const core::common::Vec2Array& pts);
    explicit SplineCurve(SourceCurveConstructionTag) {}
    void initializeSource(Vec2Array points);

    Vec2 point(float t) const override;
    float closestPoint(const Vec2& p, int samples = 100) const override;
    core::common::Vec2Array& controlPoints() override { return controlPoints_; }
    void setControlPoints(const core::common::Vec2Array& pts) override;
    Vec2 derivative(float t) const override;
    void evaluatePoints(const std::vector<float>& tSamples, core::common::Vec2Array& dst) const override;
    void evaluateDerivatives(const std::vector<float>& tSamples, core::common::Vec2Array& dst) const override;
    void evaluatePointsSource(common::SharedArray<float> samples, Vec2Array& dst) override;
    void evaluateDerivativesSource(common::SharedArray<float> samples, Vec2Array& dst) override;

    Vec2 pointSource(float t) override;
    float closestPointSource(Vec2 point, int samples = 100) override;
    void setControlPointsSource(Vec2Array& points) override;
    Vec2 derivativeSource(float t) override;
    using PointCache = common::PolicyKeyMap<float, Vec2, common::Float32KeyPolicy>;
    PointCache pointCache;
    PointCache derivativeCache;

    // Actual original public field, not a second Source copy or view.
    core::common::Vec2Array controlPoints_{};
};

std::unique_ptr<Curve> createCurve(const core::common::Vec2Array& pts, bool bezier);

// Complete's tagged constructor initializes storage only. A ported derived
// Source constructor body runs after this base construction call returns.
template<class Complete = BezierCurve, class... StorageArgs>
std::shared_ptr<Complete> ngCreateBezierCurveSource(Vec2Array points, StorageArgs&&... args) {
    static_assert(std::is_base_of_v<BezierCurve, Complete>);
    auto result = std::make_shared<Complete>(SourceCurveConstructionTag{}, std::forward<StorageArgs>(args)...);
    static_cast<BezierCurve&>(*result).initializeSource(points);
    return result;
}

template<class Complete = SplineCurve, class... StorageArgs>
std::shared_ptr<Complete> ngCreateSplineCurveSource(Vec2Array points, StorageArgs&&... args) {
    static_assert(std::is_base_of_v<SplineCurve, Complete>);
    auto result = std::make_shared<Complete>(SourceCurveConstructionTag{}, std::forward<StorageArgs>(args)...);
    static_cast<SplineCurve&>(*result).initializeSource(points);
    return result;
}

} // namespace nicxlive::core::nodes
