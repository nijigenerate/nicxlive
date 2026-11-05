#pragma once
// Inochi2D Project 2020; nijigenerate Project 2024. BSD-2-Clause, SDK LICENSE.
#include "drawable.hpp"
#include "../common/object_key_policy.hpp"
#include "../common/resource_key_map.hpp"
#include "../common/shared_array.hpp"
#include <limits>

namespace nicxlive::core::nodes {

struct ShapeNode {
    Vec2 breakpoint{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    Vec2Array shapeData;
};

class Shapes : public Node {
protected:
    common::NullableString typeId() const override;
    void runDynamicTask(core::RenderContext& context) override;
public:
    using ShapeArray = common::SharedArray<ShapeNode>;
    using ShapeMap = common::PolicyKeyMap<std::shared_ptr<Drawable>, ShapeArray,
        common::ObjectClassKeyPolicy<std::shared_ptr<Drawable>>>;
    ShapeMap shapes;
    Vec2 selector{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};

    static std::shared_ptr<Shapes> create(const std::shared_ptr<Node>& parent = {});
    virtual void processShapes();
private:
    Shapes() = default;
};

} // namespace nicxlive::core::nodes
