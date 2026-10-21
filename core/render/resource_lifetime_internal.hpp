#pragma once
// Implementation detail. Hosts use the normal update/unload lifecycle.
#include <cstdint>
#include <memory>
namespace nicxlive::core { class RenderBackend; }
namespace nicxlive::core::render::detail {
void enqueueDrawableRelease(std::weak_ptr<RenderBackend> backend, uint32_t handle);
void drainDrawableReleases();
}
