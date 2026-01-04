#pragma once

#include <functional>

namespace nicxlive::core {

// Initialize timing with a time source (seconds).
void inInit(std::function<double()> timeFunc);
void inSetTimingFunc(std::function<double()> timeFunc);
void inUpdate();

double deltaTime();
double lastTime();
double currentTime();

} // namespace nicxlive::core
