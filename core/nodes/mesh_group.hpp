#pragma once

#include "drawable.hpp"
#include "filter.hpp"
#include "deformer_base.hpp"
#include "grid_deformer.hpp"
#include "path_deformer.hpp"
#include "../compat/source_allocator.hpp"

#include <cmath>
#include <functional>
#include <utility>

namespace nicxlive::core::nodes {

struct Mat3 {
    float m[3][3]{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    float* operator[](int r) { return m[r]; }
    const float* operator[](int r) const { return m[r]; }
};

inline Mat3 operator*(const Mat3& a, const Mat3& b) {
    Mat3 out{};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            out.m[r][c] = a.m[r][0] * b.m[0][c] + a.m[r][1] * b.m[1][c] + a.m[r][2] * b.m[2][c];
        }
    }
    return out;
}

struct TriangleMapping {
    Mat3 offsetMatrices{};
    Mat3 transformMatrix{};
};

static constexpr int kMeshGroupFilterStage = 0;

class MeshGroup : public Drawable, public NodeFilter {
    template<class> friend struct d_runtime::SourceAllocator;
    static std::shared_ptr<MeshGroup> allocateSourceMeshGroupReceiver();
public:
    std::weak_ptr<Node> owner{};
    std::vector<NodeId> members{};

    // D相当のフィールド
    std::vector<uint16_t> bitMask{};
    Vec4 bounds{};
    std::vector<TriangleMapping> triangles{};
    Vec2Array transformedVertices{};
    Mat4 forwardMatrix{Mat4::identity()};
    Mat4 inverseMatrix{Mat4::identity()};
    bool translateChildren{true};
    bool dynamic{false};
    bool precalculated{false};

    MeshGroup();
    using Node::create;
    static std::shared_ptr<MeshGroup> create(const std::shared_ptr<Node>& parent = {});
    template<class ParentProducer>
    static std::shared_ptr<MeshGroup> createSourceDeferred(ParentProducer&& makeParent) {
        auto result = allocateSourceMeshGroupReceiver();
        std::shared_ptr<Node> parent = std::invoke(std::forward<ParentProducer>(makeParent));
        result->constructMeshGroupOwned(parent);
        return result;
    }
    explicit MeshGroup(const std::shared_ptr<Node>& parent);

    common::NullableString typeId() const override {
        return common::NullableString::fromStaticLiteral("MeshGroup");
    }

    // NodeFilter
    void captureTarget(const std::shared_ptr<Node>& target) override { addChild(target); }
    void releaseTarget(const std::shared_ptr<Node>& target) override;
    void dispose() override;
    void applyDeformToChildren(common::SharedArray<std::shared_ptr<core::param::Parameter>> params, bool recursive = true) override;

    void addChild(const std::shared_ptr<Node>& child);
    void clearChildren();
    void setParent(const std::shared_ptr<Node>& p);
    std::shared_ptr<Node> parentPtr() const;

    void preProcess() override;
    void postProcess(int id = 0) override;
    void runPreProcessTask(core::RenderContext& ctx) override;
    void runRenderTask(core::RenderContext& ctx) override;
    void draw() override;
    void renderMask(bool dodge = false);

    void rebuffer(const MeshData& data) override;
    void switchMode(bool dynamic);
    virtual bool getTranslateChildren() { return translateChildren; }
    void setTranslateChildren(bool value);

    void serializeSelfImpl(::nicxlive::core::serde::TypedJsonWriter& serializer, bool recursive = true,
        SerializeNodeFlags flags = SerializeNodeFlags::All) const override;
    ::nicxlive::core::serde::ReadError deserializeFromFghj(::nicxlive::core::serde::TypedJsonView data) override;
    void serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const override;
    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;

    bool setupChild(const std::shared_ptr<Node>& child) override;
    bool releaseChild(const std::shared_ptr<Node>& child) override;
    void setupSelf() override;
    void clearCache() override;
    void centralize() override;
    void copyFrom(const Node& src, bool clone = false, bool deepCopy = true) override;
    void build(bool force = false) override;
    bool coverOthers() const override { return true; }
    bool mustPropagate() const override { return false; }

protected:
    explicit MeshGroup(DeferredConstruction) : Drawable(DeferredConstruction{}) {}
    void constructMeshGroupOwned(std::shared_ptr<Node> parent) {
        constructDrawableOwned(parent);
        requirePreProcessTask();
    }

private:
    void precalculate();
    void setupChildNoRecurse(const std::shared_ptr<Node>& node, bool prepend = false);
    void releaseChildNoRecurse(const std::shared_ptr<Node>& node);
    Node::DeformFilterResult filterChildren(const std::shared_ptr<Node>& target,
                                            const Vec2Array& origVertices,
                                            Vec2Array& origDeformation,
                                            const Mat4* origTransform);
    static bool pointInTriangle(const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c);
};

} // namespace nicxlive::core::nodes
