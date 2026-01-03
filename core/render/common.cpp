#include "common.hpp"

namespace nicxlive::core {

namespace {
std::shared_ptr<RenderBackend> gCurrentBackend{};
}

std::shared_ptr<RenderBackend> getCurrentRenderBackend() {
    return gCurrentBackend;
}

void setCurrentRenderBackend(const std::shared_ptr<RenderBackend>& backend) {
    gCurrentBackend = backend;
}

} // namespace nicxlive::core
