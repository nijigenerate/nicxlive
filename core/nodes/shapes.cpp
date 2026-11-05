// Inochi2D Project 2020; nijigenerate Project 2024. BSD-2-Clause, SDK LICENSE.
#include "shapes.hpp"
#include "../compat/object_metadata.hpp"
#include <cmath>

namespace nicxlive::core::nodes {

std::shared_ptr<Shapes> Shapes::create(const std::shared_ptr<Node>& parent) {
    const auto actualParent = parent;
    d_runtime::ngRegisterObjectClassName(typeid(Shapes),
        d_runtime::RetainedString::fromStaticLiteral("nijilive.core.nodes.shapes.Shapes"));
    auto result = std::shared_ptr<Shapes>(new Shapes);
    result->constructWithParent(actualParent);
    result->requireDynamicTask();
    return result;
}

common::NullableString Shapes::typeId() const {
    return common::NullableString::fromStaticLiteral("Shapes");
}

void Shapes::processShapes() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto capturedShapes = shapes;
    for (const auto& entry : capturedShapes) {
        const auto part = entry.first;
        const auto nodes = entry.second;
        const auto nodeLen = nodes.size();
        common::SharedArray<float> weights(nodeLen, std::numeric_limits<float>::quiet_NaN());
        float accumulatedWeight = 0.0f;

        for (std::size_t i = 0; i < nodes.size(); ++i) {
            // inmath distance: float vector difference/squares, real sum/sqrt,
            // then conversion back to the vector's float component type.
            // MIT notice: core/licenses/inmath-transform-source.txt.
            const auto breakpoint = nodes[i].breakpoint;
            const auto position = selector;
            const float dx = breakpoint.x - position.x;
            const float dy = breakpoint.y - position.y;
            const float xx = dx * dx;
            const float yy = dy * dy;
            long double squared = 0.0L;
            squared += xx;
            squared += yy;
            const float distance = static_cast<float>(std::sqrt(squared));
            weights[i] = static_cast<float>(1.5 - static_cast<double>(distance) / 1.5);
            accumulatedWeight += weights[i];
        }

        if (accumulatedWeight > 1.0f) {
            for (std::size_t i = 0; i < weights.size(); ++i) {
                weights[i] /= static_cast<float>(nodeLen);
            }
        }

        const auto count = d_runtime::ngRequireObject(part)->getVertices().size();
        Vec2Array vertices(count);
        for (std::size_t i = 0; i < vertices.size(); ++i) vertices[i] = Vec2{0.0f, 0.0f};

        for (const auto& nodeValue : nodes) {
            auto node = nodeValue;
            for (std::size_t i = 0; i < node.shapeData.size(); ++i) {
                auto output = vertices[i];
                const auto weight = weights[i];
                const Vec2 offset = node.shapeData[i];
                const Vec2 term{weight * offset.x, weight * offset.y};
                const Vec2 previous = output;
                output = Vec2{previous.x + term.x, previous.y + term.y};
            }
        }
        // The authored body ends here: it does not publish or apply vertices.
    }
}

void Shapes::runDynamicTask(core::RenderContext& context) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    processShapes();
    Node::runDynamicTask(context);
}

} // namespace nicxlive::core::nodes
