#pragma once

#include "part.hpp"

namespace nicxlive::core::nodes {

class Mask : public Part {
public:
    Mask();
    using Node::create;
    static std::shared_ptr<Mask> create(const std::shared_ptr<Node>& parent = {});
    explicit Mask(const MeshData& data, uint32_t uuidVal = 0);

    common::NullableString typeId() const override;

    void fillDrawPacket(const Node& header, PartDrawPacket& packet, bool isMask = false) const override;
    void serializeSelfImpl(::nicxlive::core::serde::TypedJsonWriter& serializer, bool recursive = true,
        SerializeNodeFlags flags = SerializeNodeFlags::All) const override;
    ::nicxlive::core::serde::ReadError deserializeFromFghj(::nicxlive::core::serde::TypedJsonView data) override;
    void serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const override;
    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;
    void renderMask(bool dodge = false) override;
    void fillMaskDrawPacket(::nicxlive::core::nodes::MaskDrawPacket& packet) const;
    void runRenderTask(core::RenderContext& ctx) override;
    void draw() override;

protected:
    explicit Mask(DeferredConstruction) : Part(DeferredConstruction{}) {}
    void constructMaskOwned(std::shared_ptr<Node> parent) {
        MeshData empty;
        const auto id = Node::inCreateUUID();
        constructPartDataOwned(empty, id, parent);
    }
};

} // namespace nicxlive::core::nodes
