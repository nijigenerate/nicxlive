#include "../core/nodes/composite.hpp"
#include "../core/nodes/projectable.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

using nicxlive::core::math::Vec2;
using nicxlive::core::math::Vec3;
using nicxlive::core::math::Vec4;
using nicxlive::core::nodes::Composite;
using nicxlive::core::nodes::Projectable;

namespace {

bool nearlyEqual(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

void requireNear(const char* label, float actual, float expected) {
    if (!nearlyEqual(actual, expected)) {
        std::fprintf(stderr, "%s: expected %.6f, actual %.6f\n", label, expected, actual);
        std::fflush(stderr);
        std::exit(1);
    }
}

void requireTrue(const char* label, bool value) {
    if (!value) {
        std::fprintf(stderr, "%s: expected true\n", label);
        std::fflush(stderr);
        std::exit(1);
    }
}

void appendUniformDeformation(Projectable& node, Vec2 offset) {
    node.deformation.append(offset);
    node.deformation.append(offset);
    node.deformation.append(offset);
    node.deformation.append(offset);
}

class FixedBoundsProjectable : public Projectable {
public:
    Vec4 fixedBounds{};
    Vec2 fixedDeformOffset{};

    Vec4 getChildrenBounds(bool = true) override {
        return fixedBounds;
    }

    Vec2 deformationTranslationOffset() const override {
        return fixedDeformOffset;
    }
};

class FixedBoundsComposite : public Composite {
public:
    Vec4 fixedBounds{};

    Vec4 getChildrenBounds(bool = true) override {
        return fixedBounds;
    }
};

void testProjectableTextureOffsetKeepsUniformDeformationOffset() {
    FixedBoundsProjectable node;
    node.fixedBounds = Vec4{10.0f, 20.0f, 50.0f, 80.0f};
    node.fixedDeformOffset = Vec2{7.0f, -3.0f};
    node.localTransform.translation = Vec3{100.0f, 200.0f, 0.0f};
    node.localTransform.update();

    bool resized = node.createSimpleMesh();

    requireTrue("projectable createSimpleMesh resized", resized);
    requireNear("projectable textureOffset.x", node.textureOffset.x, -63.0f);
    requireNear("projectable textureOffset.y", node.textureOffset.y, -153.0f);
}

void testCompositeTextureOffsetKeepsUniformDeformationOffset() {
    auto node = std::make_shared<FixedBoundsComposite>();
    node->fixedBounds = Vec4{-20.0f, 10.0f, 40.0f, 70.0f};
    appendUniformDeformation(*node, Vec2{5.0f, 9.0f});
    node->localTransform.translation = Vec3{30.0f, -40.0f, 0.0f};
    node->localTransform.update();

    bool resized = node->createSimpleMesh();

    requireTrue("composite createSimpleMesh resized", resized);
    requireNear("composite textureOffset.x", node->textureOffset.x, -15.0f);
    requireNear("composite textureOffset.y", node->textureOffset.y, 89.0f);
}

} // namespace

int main() {
    testProjectableTextureOffsetKeepsUniformDeformationOffset();
    testCompositeTextureOffsetKeepsUniformDeformationOffset();
    return 0;
}
