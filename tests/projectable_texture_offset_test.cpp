#include "../core/nodes/composite.hpp"
#include "../core/nodes/projectable.hpp"
#include "../core/nodes/simple_physics_driver.hpp"
#include "../core/serde.hpp"
#include "../core/timing.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

using nicxlive::core::math::Vec2;
using nicxlive::core::math::Vec3;
using nicxlive::core::math::Vec4;
using nicxlive::core::nodes::Composite;
using nicxlive::core::nodes::ParamMapMode;
using nicxlive::core::nodes::PhysicsModel;
using nicxlive::core::nodes::Projectable;
using nicxlive::core::nodes::SimplePhysicsDriver;

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

void testProjectableTextureOffsetUsesOriginOffset() {
    FixedBoundsProjectable node;
    node.fixedBounds = Vec4{10.0f, 20.0f, 50.0f, 80.0f};
    node.fixedDeformOffset = Vec2{7.0f, -3.0f};
    node.localTransform.translation = Vec3{100.0f, 200.0f, 0.0f};
    node.localTransform.update();

    bool resized = node.createSimpleMesh();

    requireTrue("projectable createSimpleMesh resized", resized);
    requireNear("projectable textureOffset.x", node.textureOffset.x, -70.0f);
    requireNear("projectable textureOffset.y", node.textureOffset.y, -150.0f);
}

void testCompositeTextureOffsetUsesOriginOffset() {
    auto node = std::make_shared<FixedBoundsComposite>();
    node->fixedBounds = Vec4{-20.0f, 10.0f, 40.0f, 70.0f};
    appendUniformDeformation(*node, Vec2{5.0f, 9.0f});
    node->localTransform.translation = Vec3{30.0f, -40.0f, 0.0f};
    node->localTransform.update();

    bool resized = node->createSimpleMesh();

    requireTrue("composite createSimpleMesh resized", resized);
    requireNear("composite textureOffset.x", node->textureOffset.x, -20.0f);
    requireNear("composite textureOffset.y", node->textureOffset.y, 80.0f);
}

void testSimplePhysicsDeserializesDEnumNames() {
    nicxlive::core::serde::Fghj data;
    data.put("model_type", "SpringPendulum");
    data.put("map_mode", "XY");

    SimplePhysicsDriver driver;
    auto err = driver.deserializeFromFghj(data);

    requireTrue("simple physics deserialize D enum names", !err.has_value());
    requireTrue("simple physics model SpringPendulum", driver.modelType == PhysicsModel::SpringPendulum);
    requireTrue("simple physics map XY", driver.mapMode == ParamMapMode::XY);
}

void testSimplePhysicsSpringPendulumRecoversFromZeroLength() {
    double now = 0.0;
    nicxlive::core::inSetTimingFunc([&now]() {
        return now;
    });
    nicxlive::core::inUpdate();

    SimplePhysicsDriver driver;
    driver.modelType = PhysicsModel::SpringPendulum;
    driver.anchor = Vec2{0.0f, 0.0f};
    driver.length = 0.0f;
    driver.reset();

    driver.length = 100.0f;
    now = 0.01;
    nicxlive::core::inUpdate();
    driver.updateDriver();

    requireTrue("simple physics zero-length output finite",
                std::isfinite(driver.output.x) && std::isfinite(driver.output.y));
    requireTrue("simple physics zero-length output recovers along +Y",
                driver.output.y > driver.anchor.y);
}

void testSimplePhysicsResetIgnoresStaleLengthOffset() {
    double now = 0.0;
    nicxlive::core::inSetTimingFunc([&now]() {
        return now;
    });
    nicxlive::core::inUpdate();

    SimplePhysicsDriver driver;
    driver.modelType = PhysicsModel::SpringPendulum;
    driver.anchor = Vec2{0.0f, 0.0f};
    driver.length = 100.0f;
    requireTrue("simple physics length offset accepted", driver.setValue("length", 50.0f));

    driver.reset();
    driver.updateDriver();

    requireTrue("simple physics reset output finite",
                std::isfinite(driver.output.x) && std::isfinite(driver.output.y));
    requireNear("simple physics reset output ignores length offset", driver.output.y, 100.0f);
}

void testSimplePhysicsInitializesAtResolvedAnchor() {
    double now = 0.0;
    nicxlive::core::inSetTimingFunc([&now]() {
        return now;
    });
    nicxlive::core::inUpdate();

    SimplePhysicsDriver driver;
    driver.modelType = PhysicsModel::SpringPendulum;
    driver.localOnly = true;
    driver.length = 100.0f;
    driver.reset();

    driver.localTransform.translation = Vec3{30.0f, 40.0f, 0.0f};
    driver.localTransform.update();

    now = 0.01;
    nicxlive::core::inUpdate();
    driver.updateDriver();
    const float initialX = driver.output.x;
    const float initialY = driver.output.y;

    now = 0.02;
    nicxlive::core::inUpdate();
    driver.updateDriver();

    requireNear("simple physics initial anchor output.x", initialX, 30.0f);
    requireNear("simple physics initial anchor output.y", initialY, 140.0f);
    requireNear("simple physics initial anchor remains stable.x", driver.output.x, initialX);
    requireNear("simple physics initial anchor remains stable.y", driver.output.y, initialY);
}

} // namespace

int main() {
    testProjectableTextureOffsetUsesOriginOffset();
    testCompositeTextureOffsetUsesOriginOffset();
    testSimplePhysicsDeserializesDEnumNames();
    testSimplePhysicsSpringPendulumRecoversFromZeroLength();
    testSimplePhysicsResetIgnoresStaleLengthOffset();
    testSimplePhysicsInitializesAtResolvedAnchor();
    return 0;
}
