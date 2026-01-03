#pragma once

#include "node.hpp"

namespace nicxlive::core::nodes {

class Driver : public Node {
public:
    Driver() = default;

    virtual void updateDriver() = 0;
    virtual void reset() = 0;

    void runBeginTask(core::RenderContext&) override {
        // placeholder hook
    }
};

} // namespace nicxlive::core::nodes
