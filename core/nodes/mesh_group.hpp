#pragma once

#include "drawable.hpp"
#include "filter.hpp"
#include "deformer_base.hpp"
#include "grid_deformer.hpp"
#include "path_deformer.hpp"

#include <cmath>

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
    explicit MeshGroup(const std::shared_ptr<Node>& parent);

    const std::string& typeId() const override {
        static const std::string k = "MeshGroup";
        return k;
    }

    // NodeFilter
    void captureTarget(const std::shared_ptr<Node>& target) override { addChild(target); }
    void releaseTarget(const std::shared_ptr<Node>& target) override;
    void applyDeformToChildren(const std::vector<std::shared_ptr<core::param::Parameter>>& params, bool recursive = true) override;

    void addChild(const std::shared_ptr<Node>& child);
    void clearChildren();
    void setParent(const std::shared_ptr<Node>& p);
    std::shared_ptr<Node> parentPtr() const;

    void preProcess();
    void postProcess(int id = 0);
    void runPreProcessTask(core::RenderContext& ctx) override;
    void runRenderTask(core::RenderContext& ctx) override;
    void draw() override;
    void renderMask(bool dodge = false);

    void rebuffer(const MeshData& data);
    void switchMode(bool dynamic);
    void setTranslateChildren(bool value);

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

private:
    void precalculate();
    void setupChildNoRecurse(const std::shared_ptr<Node>& node, bool prepend = false);
    void releaseChildNoRecurse(const std::shared_ptr<Node>& node);
    std::tuple<Vec2Array, std::optional<Mat4>, bool> filterChildren(const std::shared_ptr<Node>& target,
                                                                    const Vec2Array& origVertices,
                                                                    const Vec2Array& origDeformation,
                                                                    const Mat4* origTransform);
    static bool pointInTriangle(const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c);
};

} // namespace nicxlive::core::nodes
