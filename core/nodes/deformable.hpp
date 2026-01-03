#pragma once

#include "node.hpp"
#include "../common/utils.hpp"
#include "../param/parameter.hpp"

#include <array>
#include <algorithm>
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
    Vec2Array pending_{};
};

class Deformable : public Node {
public:
    Vec2Array vertices{};
    Vec2Array deformation{};
    DeformationStack deformStack{this};

    Deformable();
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

    void runBeginTask(core::RenderContext& ctx) override;

    void runPreProcessTask(core::RenderContext& ctx) override;

    void runPostTaskImpl(std::size_t priority, core::RenderContext& ctx) override;

    void notifyDeformPushed(const Vec2Array& deform);

protected:
    virtual void onDeformPushed(const Vec2Array&) {}
};

} // namespace nicxlive::core::nodes
