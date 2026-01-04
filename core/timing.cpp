#include "timing.hpp"

#include <chrono>
#include <mutex>

namespace nicxlive::core {

namespace {
std::function<double()> gTimeFunc{};
double gCurrentTime{0.0};
double gLastTime{0.0};
double gDeltaTime{0.0};
std::mutex gMutex;

double defaultTimeFunc() {
    using namespace std::chrono;
    auto now = steady_clock::now().time_since_epoch();
    return duration_cast<duration<double>>(now).count();
}
} // namespace

void inInit(std::function<double()> timeFunc) {
    std::lock_guard<std::mutex> lock(gMutex);
    gTimeFunc = timeFunc ? timeFunc : defaultTimeFunc;
    gCurrentTime = 0.0;
    gLastTime = 0.0;
    gDeltaTime = 0.0;
}

void inSetTimingFunc(std::function<double()> timeFunc) {
    std::lock_guard<std::mutex> lock(gMutex);
    gTimeFunc = timeFunc ? timeFunc : defaultTimeFunc;
}

void inUpdate() {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!gTimeFunc) gTimeFunc = defaultTimeFunc;
    gCurrentTime = gTimeFunc();
    gDeltaTime = gCurrentTime - gLastTime;
    gLastTime = gCurrentTime;
}

double deltaTime() {
    std::lock_guard<std::mutex> lock(gMutex);
    return gDeltaTime;
}

double lastTime() {
    std::lock_guard<std::mutex> lock(gMutex);
    return gLastTime;
}

double currentTime() {
    std::lock_guard<std::mutex> lock(gMutex);
    return gCurrentTime;
}

} // namespace nicxlive::core
