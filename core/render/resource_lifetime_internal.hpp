#pragma once
// Implementation detail. Hosts use the normal update/unload lifecycle.
#include <cstddef>
#include <memory>
namespace nicxlive::core { class RenderBackend; }
namespace nicxlive::core::render::detail {
void enqueueDrawableRelease(std::weak_ptr<RenderBackend> backend, std::size_t handle);
void drainDrawableReleases();
}
