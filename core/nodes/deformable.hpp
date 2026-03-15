#pragma once

#include "node.hpp"
#include "../common/utils.hpp"
#include "../param/parameter.hpp"

#include <array>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <utility>
#include <optional>

namespace nicxlive::core::nodes {

using ::nicxlive::core::common::Vec2Array;
using ::nicxlive::core::param::DeformationParameterBinding;

class Deformable;

struct Deformation {
    Vec2Array vertexOffsets{};

    void update(const Vec2Array& points);

    Deformation operator-() const;
    Deformation operator+(const Deformation& other) const;
    Deformation operator-(const Deformation& other) const;
    Deformation operator*(float s) const;
};

class DeformationStack {
public:
    explicit DeformationStack(Deformable* owner);

    void preUpdate();

    void update();

    void push(const Deformation& deform);

private:
    Deformable* owner_{};
};

class Deformable : public Node {
public:
    Vec2Array vertices{};
    Vec2Array deformation{};
    DeformationStack deformStack{this};
    std::vector<Node::DeformFilterHook> deformPreProcessFilters{};
    std::vector<Node::DeformFilterHook> deformPostProcessFilters{};

    Deformable();
    explicit Deformable(const std::shared_ptr<Node>& parent);
    Deformable(uint32_t uuid, const std::shared_ptr<Node>& parent = nullptr);
    explicit Deformable(Vec2Array verts);

    virtual void updateVertices() {}
    virtual void updateBounds();

    virtual void refresh();
    virtual void refreshDeform();

    virtual void rebuffer(const Vec2Array& verts);

    virtual void pushDeformation(const Deformation& delta);

    bool mustPropagate() const override;

    virtual void updateDeform();

    virtual void remapDeformationBindings(const std::vector<std::size_t>& remap, const Vec2Array& replacement, std::size_t newLength);

    void preProcess() override;
    void postProcess(int id = 0) override;
    void copyFrom(const Node& src, bool clone = false, bool deepCopy = true) override;

    void runBeginTask(core::RenderContext& ctx) override;

    void runPreProcessTask(core::RenderContext& ctx) override;

    void runPostTaskImpl(std::size_t priority, core::RenderContext& ctx) override;

    void notifyDeformPushed(const Vec2Array& deform);
    bool hasDeformPreProcessFilter(int stage, std::uintptr_t tag) const;
    bool hasDeformPostProcessFilter(int stage, std::uintptr_t tag) const;
    void upsertDeformPreProcessFilter(Node::DeformFilterHook hook, bool prepend = false);
    void upsertDeformPostProcessFilter(Node::DeformFilterHook hook, bool prepend = false);
    void removeDeformPreProcessFilter(int stage, std::uintptr_t tag);
    void removeDeformPostProcessFilter(int stage, std::uintptr_t tag);

protected:
    virtual void onDeformPushed(const Vec2Array&) {}
};

bool areDeformationNodesCompatible(const std::shared_ptr<Node>& lhs, const std::shared_ptr<Node>& rhs);

} // namespace nicxlive::core::nodes
