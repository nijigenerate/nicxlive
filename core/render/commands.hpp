#pragma once

#include "part_draw_packet.hpp"
#include "render_pass.hpp"
#include "common.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace nicxlive::core::render {

namespace nodes {
class Part;
class Mask;
class Drawable;
class Composite;
} // namespace nodes
using ::nicxlive::core::nodes::Vec2;
using ::nicxlive::core::nodes::Vec3;
using ::nicxlive::core::nodes::BlendMode;
using ::nicxlive::core::nodes::PartDrawPacket;

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
    Vec3 tint{};
    Vec3 screenTint{};
    BlendMode blendingMode{BlendMode::Normal};
};

struct QueuedCommand {
    RenderCommandKind kind{RenderCommandKind::DrawPart};
    PartDrawPacket partPacket{};
    MaskApplyPacket maskApplyPacket{};
    ::nicxlive::core::DynamicCompositePass dynamicPass{};
    bool usesStencil{false};
};

PartDrawPacket makePartDrawPacket(const std::shared_ptr<nodes::Part>& part, bool isMask = false);
PartDrawPacket makeMaskDrawPacket(const std::shared_ptr<nodes::Mask>& mask);
bool tryMakeMaskApplyPacket(const std::shared_ptr<nodes::Drawable>& drawable, bool isDodge, MaskApplyPacket& packet);
CompositeDrawPacket makeCompositeDrawPacket(const std::shared_ptr<nodes::Composite>& composite);

} // namespace nicxlive::core::render
