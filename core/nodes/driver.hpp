#pragma once

#include "node.hpp"
#include "../param/parameter.hpp"
#include "../common/shared_array.hpp"
#include "../compat/native_access_fault.hpp"

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


    virtual std::vector<std::shared_ptr<core::param::Parameter>> getAffectedParameters() const { return {}; }

    virtual bool affectsParameter(const std::shared_ptr<core::param::Parameter>& param) const {
        if (!param) return false;
        for (auto& p : getAffectedParameters()) {
            if (p && p->uuid == param->uuid) return true;
        }
        return false;
    }

    virtual void drawDebug() {}

    // Complete Source contract; native const/vector slots above stay separate.
    virtual common::SharedArray<std::shared_ptr<core::param::Parameter>> getAffectedParametersSource() {
        return {};
    }
    virtual bool affectsParameterSource(std::shared_ptr<core::param::Parameter>& parameter) final {
        [[maybe_unused]] const auto lifetime = weak_from_this().lock();
        const auto affected = getAffectedParametersSource();
        const auto length = affected.size();
        for (std::size_t i = 0; i < length; ++i) {
            // Source foreach(ref Parameter p) observes actual array cells. The
            // first virtual getter may replace that cell or the caller's ref.
            const auto current = d_runtime::ngRequireObject(affected[i]);
            const auto currentUUID = current->getUUID();
            const auto requested = d_runtime::ngRequireObject(parameter);
            const auto requestedUUID = requested->getUUID();
            if (currentUUID == requestedUUID) return true;
        }
        return false;
    }

protected:
    // Source derived factories invoke the original base body once on their
    // already-owned complete object, rather than replaying a native ctor.
    void constructDriverOwned(std::uint32_t uuidValue, std::shared_ptr<Node> parent = {}) {
        [[maybe_unused]] const auto lifetime = weak_from_this().lock();
        Node::constructWithParent(uuidValue, parent);
    }
};

} // namespace nicxlive::core::nodes

// Original drivers/package.d publicly exports the concrete SimplePhysics type.
#include "simple_physics_driver.hpp"
