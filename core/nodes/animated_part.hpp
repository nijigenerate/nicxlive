#pragma once
// Independent nijilive/core/nodes/part/apart.d, nijigenerate Project.
// BSD-2-Clause; see the SDK LICENSE.
#include "part.hpp"
#include "../math/types.hpp"
#include "../compat/object_metadata.hpp"

namespace nicxlive::core::nodes {

class AnimatedPart : public Part {
protected:
    common::NullableString typeId() const override {
        return common::NullableString::fromStaticLiteral("AnimatedPart");
    }
public:
    math::Vec2i splits{};

    // Source's implicit no-argument constructor calls Part's default-parent body.
    static std::shared_ptr<AnimatedPart> create() {
        d_runtime::ngRegisterObjectClassName(typeid(AnimatedPart),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.nodes.part.apart.AnimatedPart"));
        auto result = std::shared_ptr<AnimatedPart>(new AnimatedPart);
        result->constructPartOwned(nullptr);
        return result;
    }
private:
    AnimatedPart() : Part(DeferredConstruction{}) {}
};

} // namespace nicxlive::core::nodes
