#pragma once

#include "render/common.hpp"
#include "render/graph_builder.hpp"
#include "render/scheduler.hpp"
#include "render/backend_queue.hpp"

namespace nicxlive::core {

class RenderContext {
public:
    RenderGraphBuilder* renderGraph{nullptr};
    RenderBackend* renderBackend{nullptr};
    RenderGpuState gpuState{};
};

} // namespace nicxlive::core
