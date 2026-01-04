#include "mask.hpp"
#include "../render/common.hpp"
#include "../puppet.hpp"

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
    for (auto& t : packet.textureUUIDs) t = 0;
    packet.vertices.clear();
    packet.uvs.clear();
    packet.indices.clear();
}

void Mask::serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const {
    // Keep identical to Drawable serialization in D implementation.
    Drawable::serializeSelfImpl(serializer, recursive, flags);
}

::nicxlive::core::serde::SerdeException Mask::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    return Drawable::deserializeFromFghj(data);
}

void Mask::renderMask(bool /*dodge*/) {
    auto backend = ::nicxlive::core::getCurrentRenderBackend();
    if (!backend) return;
    PartDrawPacket packet{};
    fillMaskDrawPacket(packet);
    ::nicxlive::core::RenderBackend::MaskApplyPacket mp;
    mp.kind = ::nicxlive::core::RenderBackend::MaskDrawableKind::Mask;
    mp.maskPacket = packet;
    mp.isDodge = false;
    backend->applyMask(mp);
}

void Mask::fillMaskDrawPacket(PartDrawPacket& packet) const {
    packet.modelMatrix = immediateModelMatrix();
    if (auto pup = puppetRef()) {
        packet.puppetMatrix = pup->transform.toMat4();
    }
    packet.renderable = false;
    packet.origin = mesh->origin;
    packet.vertexOffset = vertexOffset;
    packet.vertexAtlasStride = sharedVertexBufferData().size();
    packet.uvOffset = uvOffset;
    packet.uvAtlasStride = sharedUvBufferData().size();
    packet.deformOffset = deformOffset;
    packet.deformAtlasStride = sharedDeformBufferData().size();
    packet.indexCount = static_cast<uint32_t>(mesh->indices.size());
    packet.vertexCount = static_cast<uint32_t>(mesh->vertices.size());
    for (auto& t : packet.textureUUIDs) t = 0;
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
