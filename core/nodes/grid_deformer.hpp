#pragma once

#include "deformer_base.hpp"
#include "deformable.hpp"
#include "filter.hpp"
#include "../param/parameter.hpp"
#include "../serde.hpp"
#include "../compat/source_allocator.hpp"
#include <functional>
#include <utility>

#include <vector>
#include <bit>
#include "../compat/array_range_error.hpp"
#include "../compat/static_error.hpp"
#include <optional>
#include <limits>

namespace nicxlive::core::nodes {

enum class GridFormation {
    Bilinear,
};

// The independent module-level value type; invalid cells retain float.init.
struct GridCellCache {
    std::uint16_t cellX{0};
    std::uint16_t cellY{0};
    float u = std::numeric_limits<float>::quiet_NaN();
    float v = std::numeric_limits<float>::quiet_NaN();
    bool valid{false};
};

class GridDeformer : public Deformable, public Deformer, public SourceDeformation {
public:
    using Axis = common::SharedArray<float>;
    static constexpr float kBoundaryTolerance = 1e-4f;
    static constexpr float kAxisTolerance = 1e-4f;
    static constexpr std::array<float, 2> kDefaultAxis{-0.5f, 0.5f};

    // grid axes
    Axis axisX{};
    Axis axisY{};
    GridFormation formation{GridFormation::Bilinear};
    Mat4 inverseMatrix = [] {
        Mat4 value;
        for (auto& row : value.a.a)
            for (auto& element : row) element = std::numeric_limits<float>::quiet_NaN();
        return value;
    }();
    bool dynamic{false};
    bool translateChildren{true};

    GridDeformer();
    static std::shared_ptr<GridDeformer> create(std::shared_ptr<Node> parent = {});
    template<class ParentProducer>
    static std::shared_ptr<GridDeformer> createSourceDeferred(ParentProducer&& produceParent) {
        auto receiver = allocateSourceGridReceiver();
        std::shared_ptr<Node> parent = std::invoke(std::forward<ParentProducer>(produceParent));
        receiver->constructGridOwned(std::move(parent));
        return receiver;
    }

    // The existing native grid vertices cell is the source vertexBuffer.
    using Deformable::getVertices;
    Vec2Array& getVertices() override { return vertices; }

    common::NullableString typeId() const override {
        return common::NullableString::fromStaticLiteral("GridDeformer");
    }


    // Deformer
    DeformResult deformChildren(const std::shared_ptr<Node>& target,
                                const Vec2Array& origVertices,
                                Vec2Array& origDeformation,
                                const Mat4* origTransform) override;

    // Node overrides
    void runPreProcessTask(core::RenderContext& ctx) override;
    void runRenderTask(core::RenderContext& ctx) override;
    void build(bool force = false) override;
    bool setupChild(const std::shared_ptr<Node>& child) override;
    bool releaseChild(const std::shared_ptr<Node>& child) override;
    void captureTarget(const std::shared_ptr<Node>& target) override;
    void releaseTarget(const std::shared_ptr<Node>& target) override;
    void dispose() override;
    void clearCache() override;
    void copyFrom(const Node& src, bool clone = false, bool deepCopy = true) override;
    bool mustPropagate() const override { return false; }
    bool coverOthers() const override { return true; }

    // grid management
    void rebuffer(const Vec2Array& gridPoints) override;
    void setGridAxes(const std::vector<float>& xs, const std::vector<float>& ys);
    void setGridAxes(Axis xs, Axis ys);
    std::size_t rows() const { return axisY.size(); }
    std::size_t cols() const { return axisX.size(); }
    bool hasValidGrid() const { return rows() >= 2 && cols() >= 2; }

    void applyDeformToChildren(common::SharedArray<std::shared_ptr<core::param::Parameter>> params, bool recursive = true) override;
    void serializeSelfImpl(::nicxlive::core::serde::TypedJsonWriter& serializer, bool recursive = true,
        SerializeNodeFlags flags = SerializeNodeFlags::All) const override;
    void serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const override;
    ::nicxlive::core::serde::ReadError deserializeFromFghj(::nicxlive::core::serde::TypedJsonView data) override;
    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;

    // Appended Source slot leaves the existing native Deformer interface intact.
    SourceDeformResult deformChildrenSource(std::shared_ptr<Node> target,
        Vec2Array origVertices, Vec2Array origDeformation, Mat4* origTransform) override;

    // Public non-final Source properties, appended after existing virtual slots.
    virtual GridFormation gridFormation() const { return formation; }
    virtual void setGridFormation(GridFormation value) { formation = value; }
    virtual void switchDynamic(bool value);

protected:
    template<class> friend struct d_runtime::SourceAllocator;
    explicit GridDeformer(DeferredConstruction construction) : Deformable(construction) {}
    void constructGridOwned(std::shared_ptr<Node> parent);

private:
    static std::shared_ptr<GridDeformer> allocateSourceGridReceiver();
    void setupChildNoRecurse(const std::shared_ptr<Node>& node, bool prepend = false);
    void releaseChildNoRecurse(const std::shared_ptr<Node>& node);
    using GridCellCache = ::nicxlive::core::nodes::GridCellCache;
    struct IntervalResult {
        std::size_t index{};
        float weight = std::numeric_limits<float>::quiet_NaN();
        bool valid{false};
    };
    GridCellCache computeCache(float localX, float localY) const;
    void sampleGridPoints(Vec2Array& dst, const std::vector<GridCellCache>& caches, bool includeDeformation) const;
    void sampleGridPointsSource(Vec2Array& dst, const std::vector<GridCellCache>& caches,
        bool includeDeformation) const;
    std::optional<GridCellCache> locate(float x, float y) const;
    std::size_t gridIndex(std::size_t x, std::size_t y) const { return y * axisX.size() + x; }
    static Axis normalizeAxis(Axis values);
    int axisIndexOfValue(Axis axis, float value) const;
    void rebuildVertices();
    void rebuildBuffers();
    void adoptGridFromAxes(Axis xs, Axis ys);
    IntervalResult locateInterval(Axis axis, float value) const;
    template<class Values> static decltype(auto) sourceGridAt(Values& values,
        std::size_t index, unsigned line) {
        if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
            index, values.size(), d_runtime::RetainedString::fromStaticLiteral(
                "source/nijilive/core/nodes/deformer/grid.d"), line);
        return values[index];
    }

    template<class Points> bool deriveAxes(Points&& points, Axis& xs, Axis& ys) const {
        // D out parameters are reset before the first authored statement.
        xs = {};
        ys = {};
        const auto count = points.size();
        if (count < 4) return false;
        Axis xCandidates(count, std::numeric_limits<float>::quiet_NaN());
        Axis yCandidates(count, std::numeric_limits<float>::quiet_NaN());
        for (std::size_t i = 0; i < count; ++i) {
            auto point = points[i];
            sourceGridAt(xCandidates, i, 449) = point.x;
            sourceGridAt(yCandidates, i, 450) = point.y;
        }
        xs = normalizeAxis(xCandidates);
        ys = normalizeAxis(yCandidates);
        if (xs.size() < 2 || ys.size() < 2) return false;
        if (xs.size() * ys.size() != count) return false;
        common::SharedArray<bool> seen(xs.size() * ys.size(), false);
        for (std::size_t i = 0; i < count; ++i) {
            auto point = points[i];
            const auto xi = axisIndexOfValue(xs, point.x);
            const auto yi = axisIndexOfValue(ys, point.y);
            if (xi < 0 || yi < 0) return false;
            const auto index = static_cast<std::size_t>(yi) * xs.size() + static_cast<std::size_t>(xi);
            sourceGridAt(seen, index, 467) = true;
        }
        const auto flags = seen;
        for (auto flag : flags) if (!flag) return false;
        return true;
    }

    template<class Points> bool adoptFromVertices(Points&& points, bool preserveShape) {
        Axis xs;
        Axis ys;
        if (!deriveAxes(points, xs, ys)) return false;
        setGridAxes(xs, ys);
        if (preserveShape && !fillDeformationFromPositions(points)) return false;
        return true;
    }

    template<class Points> bool fillDeformationFromPositions(Points&& positions) {
        const auto count = positions.size();
        if (count != getDeformation().size()) {
            getDeformation().fill(Vec2{0, 0});
            return false;
        }
        common::SharedArray<bool> seen(getDeformation().size(), false);
        getDeformation().fill(Vec2{0, 0});
        for (std::size_t i = 0; i < count; ++i) {
            auto position = positions[i];
            const auto xi = axisIndexOfValue(axisX, position.x);
            const auto yi = axisIndexOfValue(axisY, position.y);
            // The original preserves the already written prefix here.
            if (xi < 0 || yi < 0) return false;
            const auto index = gridIndex(static_cast<std::size_t>(xi), static_cast<std::size_t>(yi));
            auto base = vertices[index];
            auto difference = [&] {
                if constexpr (requires { position - base; }) {
                    return position - base;
                } else {
                    // Lower the actual inmath float2 operation for native POD
                    // vectors/views, retaining open point-specific operators.
                    const Vec2 right{base.x, base.y};
                    if constexpr (requires { position - right; }) return position - right;
                    else return Vec2{position.x - right.x, position.y - right.y};
                }
            }();
            getDeformation()[index] = difference;
            sourceGridAt(seen, index, 511) = true;
        }
        const auto flags = seen;
        for (auto flag : flags) {
            if (!flag) {
                getDeformation().fill(Vec2{0, 0});
                return false;
            }
        }
        return true;
    }
    bool matrixIsFinite(const Mat4& m) const;
};

void inInitGridDeformer();

} // namespace nicxlive::core::nodes
