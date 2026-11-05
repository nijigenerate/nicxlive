#include "dynamic_composite.hpp"
#include "../compat/scope_exit.hpp"

namespace nicxlive::core::nodes {

std::shared_ptr<DynamicComposite> DynamicComposite::allocateSourceDynamicCompositeReceiver() {
    ngRegisterModelResourceClassNames();
    return std::allocate_shared<DynamicComposite>(
        d_runtime::SourceAllocator<DynamicComposite>{}, DeferredConstruction{});
}

std::shared_ptr<DynamicComposite> DynamicComposite::create(const std::shared_ptr<Node>& parent) {
    const auto actualParent = parent;
    ngRegisterModelResourceClassNames();
    auto result = std::shared_ptr<DynamicComposite>(new DynamicComposite(DeferredConstruction{}));
    result->constructDynamicCompositeOwned(actualParent);
    return result;
}

std::shared_ptr<DynamicComposite> DynamicComposite::create(MeshData data, std::uint32_t uuid,
    std::shared_ptr<Node> parent) {
    ngRegisterModelResourceClassNames();
    auto result = std::shared_ptr<DynamicComposite>(new DynamicComposite(DeferredConstruction{}));
    result->constructProjectableDataOwned(data, uuid, parent);
    return result;
}

DynamicComposite::DynamicComposite(const MeshData& data, uint32_t uuidVal) : Projectable() {
    *mesh = data;
    if (uuidVal != 0) uuid = uuidVal;
    // Legacy native sizing policy. Source construction uses the owned
    // create(data, uuid, parent) overload and its actual index-based prefix.
    autoResizedMesh = data.vertices.empty();
}

common::NullableString DynamicComposite::typeId() const {
    return common::NullableString::fromStaticLiteral("DynamicComposite");
}

// The editor uses the typed JSON path. Preserve the inherited D Projectable
// state here too, rather than falling through to Part and keeping the default.
serde::ReadError DynamicComposite::deserializeFromFghj(serde::TypedJsonView data) {
    const auto result = Part::deserializeFromFghj(data);
    textures = {};
    const auto resized = data.findFirst("auto_resized");
    if (!resized.isMissing()) {
        (void)serde::deserializeValue(resized, autoResizedMesh);
    } else {
        autoResizedMesh = mesh->indices.empty();
    }
    return result;
}

void DynamicComposite::serializeSelfImpl(serde::TypedJsonWriter& serializer,
        bool recursive, SerializeNodeFlags flags) const {
    const auto savedTextures = textures;
    d_runtime::ngRunWithScopeExit([&] {
        const_cast<DynamicComposite*>(this)->textures = {};
        Part::serializeSelfImpl(serializer, recursive, flags);
        if (has_flag(flags, SerializeNodeFlags::State)) {
            serializer.putKey("auto_resized");
            serializer.putValue(autoResizedMesh);
        }
    }, [&] { const_cast<DynamicComposite*>(this)->textures = savedTextures; });
}

} // namespace nicxlive::core::nodes
