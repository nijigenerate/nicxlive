#include "commands.hpp"

#include "../nodes/part.hpp"
#include "../nodes/mask.hpp"
#include "../nodes/composite.hpp"
#include "../nodes/drawable.hpp"
#include "../nodes/common.hpp"

#include <algorithm>
#include <cmath>

namespace nicxlive::core::render {

nodes::PartDrawPacket makePartDrawPacket(const std::shared_ptr<nodes::Part>& part, bool isMask) {
    nodes::PartDrawPacket packet{};
    if (part) {
        packet.isMask = isMask;
        part->fillDrawPacket(*part, packet, isMask);
    }
    return packet;
}

nodes::PartDrawPacket makeMaskDrawPacket(const std::shared_ptr<nodes::Mask>& mask) {
    nodes::PartDrawPacket packet{};
    if (mask) {
        mask->fillMaskDrawPacket(packet);
    }
    return packet;
}

static bool indexRangeValid(const nodes::MeshData& mesh) {
    if (mesh.indices.empty()) return true;
    std::size_t maxIdx = 0;
    for (auto idx : mesh.indices) {
        if (idx > maxIdx) maxIdx = idx;
    }
    return maxIdx < mesh.vertices.size();
}

bool tryMakeMaskApplyPacket(const std::shared_ptr<nodes::Drawable>& drawable, bool isDodge, MaskApplyPacket& packet) {
    if (!drawable) return false;
    if (auto mask = std::dynamic_pointer_cast<nodes::Mask>(drawable)) {
        packet.kind = RenderBackend::MaskDrawableKind::Mask;
        packet.maskPacket = makeMaskDrawPacket(mask);
        packet.isDodge = isDodge;
        const auto& mesh = mask->getMesh();
        if (!indexRangeValid(mesh)) return false;
        if (packet.maskPacket.indexCount == 0) return false;
        return true;
    }
    if (auto part = std::dynamic_pointer_cast<nodes::Part>(drawable)) {
        packet.kind = RenderBackend::MaskDrawableKind::Part;
        packet.partPacket = makePartDrawPacket(part, true);
        packet.isDodge = isDodge;
        const auto& mesh = part->getMesh();
        if (!indexRangeValid(mesh)) return false;
        if (packet.partPacket.indexCount == 0) return false;
        return true;
    }
    return false;
}

CompositeDrawPacket makeCompositeDrawPacket(const std::shared_ptr<nodes::Composite>& composite) {
    CompositeDrawPacket packet{};
    if (composite) {
        packet.valid = true;
        float offsetOpacity = composite->getValue("opacity");
        packet.opacity = composite->opacity * offsetOpacity;

        nodes::Vec3 clampedTint = composite->tint;
        float offsetTintR = composite->getValue("tint.r");
        float offsetTintG = composite->getValue("tint.g");
        float offsetTintB = composite->getValue("tint.b");
        if (!std::isnan(offsetTintR)) clampedTint.x = std::clamp(composite->tint.x * offsetTintR, 0.0f, 1.0f);
        if (!std::isnan(offsetTintG)) clampedTint.y = std::clamp(composite->tint.y * offsetTintG, 0.0f, 1.0f);
        if (!std::isnan(offsetTintB)) clampedTint.z = std::clamp(composite->tint.z * offsetTintB, 0.0f, 1.0f);
        packet.tint = clampedTint;

        nodes::Vec3 clampedScreenTint = composite->screenTint;
        float offsetScreenTintR = composite->getValue("screenTint.r");
        float offsetScreenTintG = composite->getValue("screenTint.g");
        float offsetScreenTintB = composite->getValue("screenTint.b");
        if (!std::isnan(offsetScreenTintR)) clampedScreenTint.x = std::clamp(composite->screenTint.x + offsetScreenTintR, 0.0f, 1.0f);
        if (!std::isnan(offsetScreenTintG)) clampedScreenTint.y = std::clamp(composite->screenTint.y + offsetScreenTintG, 0.0f, 1.0f);
        if (!std::isnan(offsetScreenTintB)) clampedScreenTint.z = std::clamp(composite->screenTint.z + offsetScreenTintB, 0.0f, 1.0f);
        packet.screenTint = clampedScreenTint;
        packet.blendingMode = composite->blendMode;
    }
    return packet;
}

} // namespace nicxlive::core::render
