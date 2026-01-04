#pragma once

#include "part.hpp"

namespace nicxlive::core::nodes {

class Mask : public Part {
public:
    Mask();
    explicit Mask(const MeshData& data, uint32_t uuidVal = 0);

    const std::string& typeId() const override;

    void fillDrawPacket(const Node& header, PartDrawPacket& packet, bool isMask = false) const override;
    void serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const override;
    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;
    void renderMask(bool dodge = false) override;
    void fillMaskDrawPacket(::nicxlive::core::nodes::MaskDrawPacket& packet) const;
    void runRenderTask(core::RenderContext& ctx) override;
    void draw() override;
};

} // namespace nicxlive::core::nodes
