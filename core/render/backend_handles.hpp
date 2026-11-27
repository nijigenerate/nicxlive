#pragma once
// Independent nijilive/core/render/backends/package.d opaque handle hierarchy.
// BSD-2-Clause; source project LICENSE applies.

namespace nicxlive::core {

struct RenderBackendHandle {
    RenderBackendHandle() = default;
    RenderBackendHandle(const RenderBackendHandle&) = delete;
    RenderBackendHandle& operator=(const RenderBackendHandle&) = delete;
    virtual ~RenderBackendHandle() = default;
};
struct RenderShaderHandle : RenderBackendHandle {};
struct RenderTextureHandle : RenderBackendHandle {};

} // namespace nicxlive::core
