#pragma once

#include <functional>

namespace nicxlive::core {

// Initialize timing with a time source (seconds).
void inInit(std::function<double()> timeFunc);
void inSetTimingFunc(std::function<double()> timeFunc);
void inUpdate();

// Original nijilive/package.d accepts a function pointer, not a delegate.
using SourceTimeFunction = double (*)();
void ngInitTimingSource(SourceTimeFunction timeFunction);
void ngSetTimingFuncSource(SourceTimeFunction timeFunction);
void ngUpdateTimingSource();

double deltaTime();
double lastTime();
double currentTime();

} // namespace nicxlive::core
