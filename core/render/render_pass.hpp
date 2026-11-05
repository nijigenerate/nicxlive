#pragma once

#include <cstddef>
#include <limits>

namespace nicxlive::core {

enum class RenderPassKind { Root, DynamicComposite };

// Render target scope hint (D: render.passes.RenderScopeHint)
struct RenderScopeHint {
    RenderPassKind kind{RenderPassKind::Root};
    std::size_t token{0};
    bool skip{false};

    static RenderScopeHint root() { return RenderScopeHint{RenderPassKind::Root, 0, false}; }
    static RenderScopeHint forDynamic(std::size_t token) {
        if (token == 0 || token == std::numeric_limits<std::size_t>::max()) return root();
        return RenderScopeHint{RenderPassKind::DynamicComposite, token, false};
    }
    static RenderScopeHint skipHint() { return RenderScopeHint{RenderPassKind::Root, 0, true}; }
};

} // namespace nicxlive::core
