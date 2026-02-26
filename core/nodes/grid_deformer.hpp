#pragma once

#include "deformer_base.hpp"
#include "deformable.hpp"
#include "filter.hpp"
#include "../param/parameter.hpp"
#include "../serde.hpp"

#include <vector>
#include <optional>

namespace nicxlive::core::nodes {

enum class GridFormation {
    Bilinear,
};

class GridDeformer : public Deformable, public Deformer {
public:
    static constexpr float kBoundaryTolerance = 1e-4f;
    static constexpr float kAxisTolerance = 1e-4f;
    static constexpr std::array<float, 2> kDefaultAxis{-0.5f, 0.5f};

    // grid axes
    std::vector<float> axisX{};
    std::vector<float> axisY{};
    GridFormation formation{GridFormation::Bilinear};
    Mat4 inverseMatrix{Mat4::identity()};
    bool dynamic{false};
    bool translateChildren{true};

    GridDeformer();

    const std::string& typeId() const override {
        static const std::string k = "GridDeformer";
        return k;
    }

    GridFormation gridFormation() const { return formation; }
    void setGridFormation(GridFormation f) { formation = f; }

    // Deformer
    DeformResult deformChildren(const std::shared_ptr<Node>& target,
                                const Vec2Array& origVertices,
                                Vec2Array origDeformation,
                                const Mat4* origTransform) override;

    // Node overrides
    void runPreProcessTask(core::RenderContext& ctx) override;
    void runRenderTask(core::RenderContext& ctx) override;
    void build(bool force = false) override;
    bool setupChild(const std::shared_ptr<Node>& child) override;
    bool releaseChild(const std::shared_ptr<Node>& child) override;
    void captureTarget(const std::shared_ptr<Node>& target) override;
    void releaseTarget(const std::shared_ptr<Node>& target) override;
    void clearCache() override;
    void copyFrom(const Node& src, bool clone = false, bool deepCopy = true) override;
    bool mustPropagate() const override { return false; }
    bool coverOthers() const override { return true; }

    // grid management
    void rebuffer(const Vec2Array& gridPoints) override;
    void switchDynamic(bool value);
    void setGridAxes(const std::vector<float>& xs, const std::vector<float>& ys);
    std::size_t rows() const { return axisY.size(); }
    std::size_t cols() const { return axisX.size(); }
    bool hasValidGrid() const { return rows() >= 2 && cols() >= 2; }

    void applyDeformToChildren(const std::vector<std::shared_ptr<core::param::Parameter>>& params, bool recursive = true) override;
    void serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const override;
    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;

private:
    void setupChildNoRecurse(const std::shared_ptr<Node>& node, bool prepend = false);
    void releaseChildNoRecurse(const std::shared_ptr<Node>& node);
    struct GridCellCache {
        std::size_t cellX{};
        std::size_t cellY{};
        float u{0};
        float v{0};
        bool valid{false};
    };
    struct IntervalResult {
        std::size_t index{};
        float weight{0};
        bool valid{false};
    };
    GridCellCache computeCache(float localX, float localY) const;
    void sampleGridPoints(Vec2Array& dst, const std::vector<GridCellCache>& caches, bool includeDeformation) const;
    std::optional<GridCellCache> locate(float x, float y) const;
    std::size_t gridIndex(std::size_t x, std::size_t y) const { return y * axisX.size() + x; }
    std::vector<float> normalizeAxis(const std::vector<float>& values) const;
    int axisIndexOfValue(const std::vector<float>& axis, float value) const;
    bool deriveAxes(const Vec2Array& points, std::vector<float>& xs, std::vector<float>& ys) const;
    bool fillDeformationFromPositions(const Vec2Array& positions);
    bool adoptFromVertices(const Vec2Array& points, bool preserveShape);
    void adoptGridFromAxes(const std::vector<float>& xs, const std::vector<float>& ys);
    IntervalResult locateInterval(const std::vector<float>& axis, float value) const;
    bool matrixIsFinite(const Mat4& m) const;
};

} // namespace nicxlive::core::nodes
