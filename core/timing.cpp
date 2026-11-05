// Independent nijilive/package.d timing Source adaptation.
// Copyright 2020 Inochi2D Project, 2024 nijigenerate Project; BSD-2-Clause, SDK LICENSE.
#include "timing.hpp"
#include "runtime_state.hpp"
#include "texture.hpp"
#include "compat/native_access_fault.hpp"

#include <utility>

namespace nicxlive::core {

namespace {
thread_local std::function<double()> gTimeFunc{};
thread_local double gCurrentTime{0.0};
thread_local double gLastTime{0.0};
thread_local double gDeltaTime{0.0};

} // namespace

void inInit(std::function<double()> timeFunc) {
    initRenderer();
    gTimeFunc = std::move(timeFunc);
}

void inSetTimingFunc(std::function<double()> timeFunc) {
    gTimeFunc = std::move(timeFunc);
}

void ngSetTimingFuncSource(SourceTimeFunction timeFunction) {
    // Store in the same actual timing cell. The tiny value closure holds only
    // the original function pointer; it is not another timing state/owner.
    gTimeFunc = [timeFunction] {
        const auto current = timeFunction;
        if (!current) d_runtime::ngFailNullAccess(false);
        return current();
    };
}

void ngInitTimingSource(SourceTimeFunction timeFunction) {
    initRenderer();
    ngSetTimingFuncSource(timeFunction);
}

void ngUpdateTimingSource() {
    // Source's default null function faults before any time field changes.
    if (!gTimeFunc) d_runtime::ngFailNullAccess(false);
    gCurrentTime = gTimeFunc();
    gDeltaTime = gCurrentTime - gLastTime;
    gLastTime = gCurrentTime;
    inDrainPendingTextureDisposals();
}

void inUpdate() {
    gCurrentTime = gTimeFunc();
    gDeltaTime = gCurrentTime - gLastTime;
    gLastTime = gCurrentTime;
    inDrainPendingTextureDisposals();
}

double deltaTime() {
    return gDeltaTime;
}

double lastTime() {
    return gLastTime;
}

double currentTime() {
    return gCurrentTime;
}

} // namespace nicxlive::core
