#include "mask.hpp"

namespace nicxlive::core::nodes {

Mask::Mask() = default;

Mask::Mask(const MeshData& data, uint32_t uuidVal)
    : Part(data, std::array<std::shared_ptr<::nicxlive::core::Texture>, 3>{}, uuidVal) {}

const std::string& Mask::typeId() const {
    static const std::string k = "Mask";
    return k;
}

void Mask::fillDrawPacket(const Node& header, PartDrawPacket& packet, bool isMask) const {
    Part::fillDrawPacket(header, packet, isMask);
    packet.renderable = false;
}

void Mask::serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const {
    // Keep identical to Drawable serialization in D implementation.
    Drawable::serializeSelfImpl(serializer, recursive, flags);
}

::nicxlive::core::serde::SerdeException Mask::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    return Drawable::deserializeFromFghj(data);
}

void Mask::renderMask(bool /*dodge*/) {
    // Rendering backend unavailable.
}

void Mask::fillMaskDrawPacket(PartDrawPacket& packet) const {
    packet.modelMatrix = immediateModelMatrix();
    packet.renderable = false;
}

void Mask::runRenderTask(core::RenderContext&) {
    // Masks never render their own color.
}

void Mask::draw() {
    if (!enabled) return;
    for (auto& child : childrenRef()) {
        if (child) child->draw();
    }
}

} // namespace nicxlive::core::nodes
