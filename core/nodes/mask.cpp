#include "mask.hpp"
#include "../render/common.hpp"
#include "../render/commands.hpp"
#include "../render/backend_queue.hpp"
#include "../puppet.hpp"
#include "../runtime_state.hpp"

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
    for (auto& t : packet.textures) t.reset();
}

void Mask::serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const {
    // Keep identical to Drawable serialization in D implementation.
    Drawable::serializeSelfImpl(serializer, recursive, flags);
}

::nicxlive::core::serde::SerdeException Mask::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    return Drawable::deserializeFromFghj(data);
}

void Mask::renderMask(bool dodge) {
    auto backend = ::nicxlive::core::getCurrentRenderBackend();
    if (!backend) return;
    ::nicxlive::core::nodes::MaskDrawPacket packet{};
    fillMaskDrawPacket(packet);
    ::nicxlive::core::RenderBackend::MaskApplyPacket mp;
    mp.kind = ::nicxlive::core::RenderBackend::MaskDrawableKind::Mask;
    mp.maskPacket = packet;
    mp.isDodge = dodge;
    backend->applyMask(mp);
}

void Mask::fillMaskDrawPacket(::nicxlive::core::nodes::MaskDrawPacket& packet) const {
    Mat4 modelMatrix = immediateModelMatrix();
    packet.modelMatrix = modelMatrix;

    if (hasOffscreenModelMatrix) {
        if (hasOffscreenRenderMatrix) {
            packet.mvp = offscreenRenderMatrix * modelMatrix;
        } else {
            auto renderSpace = currentRenderSpace();
            packet.mvp = renderSpace.matrix * modelMatrix;
        }
    } else {
        auto renderSpace = currentRenderSpace();
        packet.mvp = renderSpace.matrix * modelMatrix;
    }

    packet.origin = mesh->origin;
    packet.vertexOffset = vertexOffset;
    packet.vertexAtlasStride = sharedVertexBufferData().size();
    packet.deformOffset = deformOffset;
    packet.deformAtlasStride = sharedDeformBufferData().size();
    packet.indexBuffer = ibo;
    packet.indexCount = static_cast<uint32_t>(mesh->indices.size());
    packet.vertexCount = static_cast<uint32_t>(mesh->vertices.size());
    // Keep queue backend IBO map in sync, same contract as Part::fillDrawPacket.
    if (packet.indexBuffer != 0 && !mesh->indices.empty()) {
        if (auto qb = std::dynamic_pointer_cast<core::render::QueueRenderBackend>(core::getCurrentRenderBackend())) {
            if (!qb->hasDrawableIndices(packet.indexBuffer)) {
                qb->uploadDrawableIndices(packet.indexBuffer, mesh->indices);
            }
        }
    }
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
