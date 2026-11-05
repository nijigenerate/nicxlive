#pragma once
// Independent nijilive.core.diff_collect Source frontend.
#include "runtime_state.hpp"

namespace nicxlive::core {
inline constexpr int TileColumns = 16;
inline constexpr int TileCount = TileColumns * TileColumns;

inline void rpSetDifferenceEvaluationEnabled(bool enabled) {
    const auto backend = tryRenderBackend();
    if (backend) backend->setDifferenceAggregationEnabled(enabled);
}
inline bool rpDifferenceEvaluationEnabled() {
    const auto backend = tryRenderBackend();
    return backend ? backend->isDifferenceAggregationEnabled() : false;
}
inline void rpSetDifferenceEvaluationRegion(DifferenceEvaluationRegion region) {
    const auto backend = tryRenderBackend();
    if (backend) backend->setDifferenceAggregationRegion(region);
}
inline DifferenceEvaluationRegion rpGetDifferenceEvaluationRegion() {
    const auto backend = tryRenderBackend();
    return backend ? backend->getDifferenceAggregationRegion() : DifferenceEvaluationRegion{};
}
inline bool rpEvaluateDifference(RenderResourceHandle sourceTexture, int viewportWidth, int viewportHeight) {
    const auto backend = tryRenderBackend();
    return backend ? backend->evaluateDifferenceAggregation(sourceTexture, viewportWidth, viewportHeight) : false;
}
inline bool rpFetchDifferenceResult(DifferenceEvaluationResult& result) {
    result = DifferenceEvaluationResult{};
    const auto backend = tryRenderBackend();
    return backend ? backend->fetchDifferenceAggregationResult(result) : false;
}
} // namespace nicxlive::core
