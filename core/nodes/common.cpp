// Copyright 2020 Inochi2D Project, 2024 nijigenerate Project.
// Author: Luna Nielsen. Distributed under the 2-Clause BSD License; see LICENSE.
// Source: nijilive/core/nodes/common.d.
#include "common.hpp"
#include "../runtime_state.hpp"

namespace nicxlive::core::nodes {
namespace {
thread_local bool inAdvancedBlending = false;
thread_local bool inAdvancedBlendingCoherent = false;
#ifdef __APPLE__
constexpr bool inDefaultTripleBufferFallback = true;
#else
constexpr bool inDefaultTripleBufferFallback = false;
#endif
thread_local bool inForceTripleBufferFallback = inDefaultTripleBufferFallback;
thread_local bool inAdvancedBlendingAvailable = false;
thread_local bool inAdvancedBlendingCoherentAvailable = false;

void inApplyBlendingCapabilities() {
    const bool desiredAdvanced = inAdvancedBlendingAvailable && !inForceTripleBufferFallback;
    const bool desiredCoherent = inAdvancedBlendingCoherentAvailable && !inForceTripleBufferFallback;
    if (desiredCoherent != inAdvancedBlendingCoherent) setAdvancedBlendCoherent(desiredCoherent);
    inAdvancedBlending = desiredAdvanced;
    inAdvancedBlendingCoherent = desiredCoherent;
}
void inSetBlendModeLegacy(BlendMode mode) { setLegacyBlendMode(mode); }
}

void setAdvancedBlendCoherent(bool enabled) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
    currentRenderBackend()->setAdvancedBlendCoherent(enabled);
#endif
}
void setLegacyBlendMode(BlendMode mode) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
    currentRenderBackend()->setLegacyBlendMode(mode);
#endif
}
void setAdvancedBlendEquation(BlendMode mode) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
    currentRenderBackend()->setAdvancedBlendEquation(mode);
#endif
}
void issueBlendBarrier() {
#ifndef NICXLIVE_SOURCE_NO_RENDER
    currentRenderBackend()->issueBlendBarrier();
#endif
}
bool hasAdvancedBlendSupport() {
#ifndef NICXLIVE_SOURCE_NO_RENDER
    return currentRenderBackend()->supportsAdvancedBlend();
#else
    return false;
#endif
}
bool hasAdvancedBlendCoherentSupport() {
#ifndef NICXLIVE_SOURCE_NO_RENDER
    return currentRenderBackend()->supportsAdvancedBlendCoherent();
#else
    return false;
#endif
}
bool inUseMultistageBlending(BlendMode mode) {
    if (inForceTripleBufferFallback) return false;
    switch (mode) {
    case BlendMode::Normal:
    case BlendMode::LinearDodge:
    case BlendMode::AddGlow:
    case BlendMode::Subtract:
    case BlendMode::Inverse:
    case BlendMode::DestinationIn:
    case BlendMode::ClipToLower:
    case BlendMode::SliceFromLower: return false;
    default: return inAdvancedBlending;
    }
}
void nlApplyBlendingCapabilities() { inApplyBlendingCapabilities(); }

// This is the original unconditional Source API, not a generated test program.
void nlSetBlendingCapabilitiesForTesting(bool advancedAvailable, bool coherentAvailable, bool forceFallback) {
    inAdvancedBlendingAvailable = advancedAvailable;
    inAdvancedBlendingCoherentAvailable = coherentAvailable;
    inForceTripleBufferFallback = forceFallback;
    inApplyBlendingCapabilities();
}
void inInitBlending() {
    inForceTripleBufferFallback = inDefaultTripleBufferFallback;
    inAdvancedBlendingAvailable = hasAdvancedBlendSupport();
    inAdvancedBlendingCoherentAvailable = hasAdvancedBlendCoherentSupport();
    inApplyBlendingCapabilities();
}
void nlSetTripleBufferFallback(bool enabled) {
    if (inForceTripleBufferFallback == enabled) return;
    inForceTripleBufferFallback = enabled;
    inApplyBlendingCapabilities();
}
bool nlIsTripleBufferFallbackEnabled() { return inForceTripleBufferFallback; }
bool inIsAdvancedBlendMode(BlendMode mode) {
    if (!inAdvancedBlending) return false;
    switch (mode) {
    case BlendMode::Multiply:
    case BlendMode::Screen:
    case BlendMode::Overlay:
    case BlendMode::Darken:
    case BlendMode::Lighten:
    case BlendMode::ColorDodge:
    case BlendMode::ColorBurn:
    case BlendMode::HardLight:
    case BlendMode::SoftLight:
    case BlendMode::Difference:
    case BlendMode::Exclusion: return true;
    default: return false;
    }
}
void inSetBlendMode(BlendMode mode, bool legacyOnly) {
    if (!inAdvancedBlending || legacyOnly) inSetBlendModeLegacy(mode);
    else setAdvancedBlendEquation(mode);
}
void inBlendModeBarrier(BlendMode mode) {
    if (inAdvancedBlending && !inAdvancedBlendingCoherent && inIsAdvancedBlendMode(mode)) issueBlendBarrier();
}
} // namespace nicxlive::core::nodes
