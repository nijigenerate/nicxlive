#pragma once

#include "node.hpp"
#include "../param/parameter.hpp"

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

    const std::string& typeId() const override {
        static const std::string k = "Driver";
        return k;
    }

    virtual std::vector<std::shared_ptr<core::param::Parameter>> getAffectedParameters() const { return {}; }

    virtual bool affectsParameter(const std::shared_ptr<core::param::Parameter>& param) const {
        if (!param) return false;
        for (auto& p : getAffectedParameters()) {
            if (p && p->uuid == param->uuid) return true;
        }
        return false;
    }

    virtual void drawDebug() {}
};

} // namespace nicxlive::core::nodes
