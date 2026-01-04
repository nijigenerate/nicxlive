#include "runtime_state.hpp"

#include "render/common.hpp"

namespace nicxlive::core {

namespace {
std::vector<int> gViewportWidth{};
std::vector<int> gViewportHeight{};
std::vector<math::Camera> gCameras{};
} // namespace

void inPushViewport(int width, int height) {
    gViewportWidth.push_back(width);
    gViewportHeight.push_back(height);
    inPushCamera();
}

void inPopViewport() {
    if (gViewportWidth.size() > 1) {
        gViewportWidth.pop_back();
        gViewportHeight.pop_back();
        inPopCamera();
    }
}

void inSetViewport(int width, int height) {
    if (gViewportWidth.empty()) {
        inPushViewport(width, height);
        return;
    }
    gViewportWidth.back() = width;
    gViewportHeight.back() = height;
}

void inGetViewport(int& width, int& height) {
    if (gViewportWidth.empty()) {
        width = 0;
        height = 0;
        return;
    }
    width = gViewportWidth.back();
    height = gViewportHeight.back();
}

void inPushCamera() {
    gCameras.emplace_back();
}

void inPushCamera(const math::Camera& camera) {
    gCameras.push_back(camera);
}

void inPopCamera() {
    if (gCameras.size() > 1) {
        gCameras.pop_back();
    }
}

math::Camera& inGetCamera() {
    if (gCameras.empty()) {
        gCameras.emplace_back();
    }
    return gCameras.back();
}

void inSetCamera(const math::Camera& camera) {
    if (gCameras.empty()) {
        gCameras.push_back(camera);
    } else {
        gCameras.back() = camera;
    }
}

void initRendererCommon() {
    inPushViewport(0, 0);
}

void initRenderer() {
    initRendererCommon();
    if (auto backend = getCurrentRenderBackend()) {
        backend->initializeRenderer();
    }
}

} // namespace nicxlive::core
