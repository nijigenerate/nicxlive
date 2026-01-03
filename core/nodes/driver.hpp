#pragma once

#include "node.hpp"

namespace nicxlive::core::nodes {

class Driver : public Node {
public:
    Driver() = default;
    explicit Driver(uint32_t uuidVal, const std::shared_ptr<Node>& parent = nullptr) {
        uuid = uuidVal;
        if (parent) setParent(parent);
    }

    virtual void updateDriver() = 0;
    virtual void reset() = 0;

    void runBeginTask(core::RenderContext& ctx) override { Node::runBeginTask(ctx); }
};

} // namespace nicxlive::core::nodes
