#pragma once

#include "../nodes/part.hpp"
#include "../nodes/composite.hpp"
#include "../nodes/drawable.hpp"
#include "../nodes/mask.hpp"
#include "../texture.hpp"
#include "common.hpp"
#include "render_pass.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace nicxlive::core::render {

enum class RenderCommandKind {
    DrawPart,
    DrawMask,
    BeginDynamicComposite,
    EndDynamicComposite,
    BeginMask,
    ApplyMask,
    BeginMaskContent,
    EndMask,
    BeginComposite,
    DrawCompositeQuad,
    EndComposite,
};

enum class MaskDrawableKind {
    Part,
    Mask,
};

using MaskApplyPacket = RenderBackend::MaskApplyPacket;

struct CompositeDrawPacket {
    bool valid{false};
    float opacity{1.0f};
    nodes::Vec3 tint{};
    nodes::Vec3 screenTint{};
    nodes::BlendMode blendingMode{nodes::BlendMode::Normal};
};

struct QueuedCommand {
    RenderCommandKind kind{RenderCommandKind::DrawPart};
    nodes::PartDrawPacket partPacket{};
    MaskApplyPacket maskApplyPacket{};
    ::nicxlive::core::DynamicCompositePass dynamicPass{};
    bool usesStencil{false};
};

nodes::PartDrawPacket makePartDrawPacket(const std::shared_ptr<nodes::Part>& part, bool isMask = false);
nodes::PartDrawPacket makeMaskDrawPacket(const std::shared_ptr<nodes::Mask>& mask);
bool tryMakeMaskApplyPacket(const std::shared_ptr<nodes::Drawable>& drawable, bool isDodge, MaskApplyPacket& packet);
CompositeDrawPacket makeCompositeDrawPacket(const std::shared_ptr<nodes::Composite>& composite);

} // namespace nicxlive::core::render
