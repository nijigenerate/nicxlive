#pragma once

#include <cstddef>

namespace nicxlive::core {

enum class RenderPassKind { Root, DynamicComposite };

// Render target scope hint (D: render.passes.RenderScopeHint)
struct RenderScopeHint {
    RenderPassKind kind{RenderPassKind::Root};
    std::size_t token{0};
    bool skip{false};

    static RenderScopeHint root() { return RenderScopeHint{RenderPassKind::Root, 0, false}; }
    static RenderScopeHint forDynamic(std::size_t t) { return t == 0 ? root() : RenderScopeHint{RenderPassKind::DynamicComposite, t, false}; }
    static RenderScopeHint skipHint() { return RenderScopeHint{RenderPassKind::Root, 0, true}; }
};

} // namespace nicxlive::core
