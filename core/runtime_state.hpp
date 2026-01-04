#pragma once

#include "math/camera.hpp"

#include <vector>

namespace nicxlive::core {

// Viewport stack
void inPushViewport(int width, int height);
void inPopViewport();
void inSetViewport(int width, int height);
void inGetViewport(int& width, int& height);

// Camera stack
void inPushCamera();
void inPushCamera(const math::Camera& camera);
void inPopCamera();
math::Camera& inGetCamera();
void inSetCamera(const math::Camera& camera);

// Renderer init (D: initRendererCommon/initRenderer)
void initRendererCommon();
void initRenderer();

} // namespace nicxlive::core
