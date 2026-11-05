#pragma once

#include "composite.hpp"
#include "initialization.hpp"
#include "../compat/source_allocator.hpp"
#include <functional>
#include <utility>

namespace nicxlive::core::nodes {

class DynamicComposite : public Projectable {
    template<class> friend struct d_runtime::SourceAllocator;
    static std::shared_ptr<DynamicComposite> allocateSourceDynamicCompositeReceiver();
public:
    bool needsRedraw{false};
    bool cached{false};
    OffscreenState offscreen{};

    DynamicComposite() = default;
    using Node::create;
    static std::shared_ptr<DynamicComposite> create(const std::shared_ptr<Node>& parent = {});
    template<class ParentProducer>
    static std::shared_ptr<DynamicComposite> createSourceDeferred(ParentProducer&& makeParent) {
        auto result = allocateSourceDynamicCompositeReceiver();
        std::shared_ptr<Node> parent = std::invoke(std::forward<ParentProducer>(makeParent));
        result->constructDynamicCompositeOwned(parent);
        return result;
    }
    static std::shared_ptr<DynamicComposite> create(MeshData data, std::uint32_t uuid,
        std::shared_ptr<Node> parent = {});
    explicit DynamicComposite(const MeshData& data, uint32_t uuidVal = 0);

    common::NullableString typeId() const override;
    using Projectable::deserializeFromFghj;
    using Projectable::serializeSelfImpl;
    serde::ReadError deserializeFromFghj(serde::TypedJsonView data) override;
    void serializeSelfImpl(serde::TypedJsonWriter& serializer, bool recursive,
        SerializeNodeFlags flags) const override;

protected:
    explicit DynamicComposite(DeferredConstruction) : Projectable(DeferredConstruction{}) {}
    void constructDynamicCompositeOwned(std::shared_ptr<Node> parent) { constructProjectableOwned(parent); }
};

// Source aliases name the same functions and the same single frame counter.
inline constexpr auto& advanceDynamicCompositeFrame = ngAdvanceProjectableFrameSource;
inline constexpr auto& currentDynamicCompositeFrame = currentProjectableFrame;

} // namespace nicxlive::core::nodes
