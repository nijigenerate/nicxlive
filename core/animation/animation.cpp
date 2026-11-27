#include "animation.hpp"
#include "../puppet.hpp"
#include "../compat/assert_error.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>

namespace nicxlive::core {
namespace {
std::string_view interpolationName(common::InterpolateMode mode) {
    switch (static_cast<int>(mode)) {
    case 0: return "Nearest";
    case 1: return "Linear";
    case 2: return "Stepped";
    case 3: return "Cubic";
    case 4: return "Bezier";
    case 5: return "COUNT";
    default: d_runtime::ngAssertUnreachable("Assertion failure", "mir/serde.d", 522);
    }
}
std::string_view mergeModeName(param::ParamMergeMode mode) {
    switch (mode) {
    case param::ParamMergeMode::Additive: return "Additive";
    case param::ParamMergeMode::Weighted: return "Weighted";
    case param::ParamMergeMode::Multiplicative: return "Multiplicative";
    case param::ParamMergeMode::Forced: return "Forced";
    case param::ParamMergeMode::Passthrough: return "Passthrough";
    default: d_runtime::ngAssertUnreachable("Assertion failure", "mir/serde.d", 522);
    }
}
float lerp(float a, float b, float t) { return a * (1 - t) + b * t; }
// Formulae from inmath 1.0.6 interpolate.d, Copyright David Herberth 2011-2021.
// MIT license retained in inmath.LICENSE.
// D float ^^ 3 calls std.math.exponential.pow: real intermediates, then
// float return. Its ^^ 2 fast path instead multiplies directly in float.
float cube(float t) {
    const long double value = t;
    return static_cast<float>(value * (value * value));
}
float cubic(float p0, float p1, float p2, float p3, float t) {
    const float a = static_cast<float>(-0.5 * p0 + 1.5 * p1 - 1.5 * p2 + 0.5 * p3);
    const float b = static_cast<float>(p0 - 2.5 * p1 + 2 * p2 - 0.5 * p3);
    const float c = static_cast<float>(-0.5 * p0 + 0.5 * p2);
    const float d = p1;
    return a * cube(t) + b * (t * t) + c * t + d;
}
float hermite(float x, float tx, float y, float ty, float t) {
    const float h1 = 2 * cube(t) - 3 * (t * t) + 1;
    const float h2 = -2 * cube(t) + 3 * (t * t);
    const float h3 = cube(t) - 2 * (t * t) + t;
    const float h4 = cube(t) - t * t;
    return h1 * x + h3 * tx + h2 * y + h4 * ty;
}
}
void Keyframe::serialize(serde::TypedJsonWriter& serializer) const {
    const auto state = serializer.structBegin();
    serializer.putField("frame", frame);
    serializer.putField("value", value);
    serializer.putField("tension", tension);
    serializer.structEnd(state);
}
void AnimationLane::serialize(serde::TypedJsonWriter& serializer) const {
    auto state = serializer.structBegin();
    serializer.putKey("interpolation");
    serializer.putValue(interpolationName(interpolation));
    if (paramRef) {
        serializer.putKey("uuid");
        serializer.putValue(d_runtime::ngRequireObject(
            d_runtime::ngRequireObject(paramRef)->targetParam)->getUUID());
        serializer.putKey("target");
        serializer.putValue(d_runtime::ngRequireObject(paramRef)->targetAxis);
    }
    serializer.putKey("keyframes");
    serializer.serializeValue(frames);
    serializer.putKey("merge_mode");
    serializer.putValue(mergeModeName(mergeMode));
    serializer.structEnd(state);
}
float AnimationLane::get(float frame, bool snapSubframes) const {
    if (!frames.empty()) {
        if (snapSubframes) frame = std::floor(frame);
        if (frames.size() == 1) return frames[0].value;
        const auto frameCount = frames.size();
        for (std::size_t i = 0; i < frameCount; ++i) {
            if (static_cast<float>(frames[i].frame) < frame) continue;
            if (i == 0) return frames[0].value;
            const float tonext = static_cast<float>(frames[i].frame) - frame;
            const float ilen = static_cast<float>(frames[i].frame) - static_cast<float>(frames[i - 1].frame);
            const float t = 1 - tonext / ilen;
            const float tension = frames[i].tension;
            switch (interpolation) {
            case common::InterpolateMode::Nearest: return t > 0.5 ? frames[i].value : frames[i - 1].value;
            case common::InterpolateMode::Step: return frames[i - 1].value;
            case common::InterpolateMode::Linear: return lerp(frames[i - 1].value, frames[i].value, t);
            case common::InterpolateMode::Cubic: {
                const float prev = frames[i < 2 ? 0 : i - 2].value;
                const float curr = frames[i - 1].value;
                const float next1 = frames[i].value;
                const float next2 = frames[std::min(i + 1, frames.size() - 1)].value;
                return cubic(prev, curr, next1, next2, t);
            }
            case common::InterpolateMode::Bezier: {
                const float h = hermite(0, 2 * tension, 1, 2 * tension, t);
                return lerp(frames[i - 1].value, frames[i].value, std::min(std::max(h, 0.0f), 1.0f));
            }
            default: d_runtime::ngAssertUnreachable("Assertion failure", "nijilive/core/animation/animation.d", 253);
            }
        }
        return frames[frames.size() - 1].value;
    }
    return 0;
}
void AnimationLane::reconstruct(const std::shared_ptr<Puppet>&) {}
void AnimationLane::finalize(const std::shared_ptr<Puppet>& puppet) {
    const auto parent = puppet;
    if (paramRef) {
        // Original assignment evaluates the virtual RHS before the current LHS.
        auto target = d_runtime::ngRequireObject(parent)->findParameter(refuuid);
        d_runtime::ngRequireObject(paramRef)->targetParam = target;
    }
}
void AnimationLane::updateFrames() {
    if (frames.size() > 1) std::stable_sort(frames.begin(), frames.end(), [](const Keyframe& a, const Keyframe& b) {
        return a.frame < b.frame;
    });
}
void Animation::reconstruct(const std::shared_ptr<Puppet>& puppet) {
    const auto parent = puppet;
    auto copy = lanes.duplicate();
    for (auto& lane : copy) lane.reconstruct(parent);
}
void Animation::finalize(const std::shared_ptr<Puppet>& puppet) {
    const auto parent = puppet;
    auto capturedLanes = lanes;
    for (auto& lane : capturedLanes) lane.finalize(parent);
}
void Animation::serialize(serde::TypedJsonWriter& serializer) const {
    auto object = serializer.structBegin();
    serializer.putKey("timestep"); serializer.serializeValue(timestep);
    serializer.putKey("additive"); serializer.serializeValue(additive);
    serializer.putKey("length"); serializer.serializeValue(length);
    // The repeated authored key is intentional.
    serializer.putKey("timestep"); serializer.serializeValue(timestep);
    serializer.putKey("leadIn"); serializer.serializeValue(leadIn);
    serializer.putKey("leadOut"); serializer.serializeValue(leadOut);
    serializer.putKey("animationWeight"); serializer.serializeValue(animationWeight);
    serializer.putKey("lanes");
    auto state = serializer.listBegin();
    const auto capturedLanes = lanes;
    for (auto lane : capturedLanes) {
        if (d_runtime::ngRequireObject(lane.paramRef)->targetParam) {
            serializer.elemBegin();
            serializer.serializeValue(lane);
        }
    }
    serializer.listEnd(state);
    serializer.structEnd(object);
}
serde::ReadError Keyframe::deserializeFromFghj(serde::TypedJsonView data) {
    if (data.kind() != serde::JsonKind::Object) {
        const auto error = *serde::unexpectedJsonKind(data);
        return "Cann't deserialize Keyframe. Unexpected data: " + error.substr(std::string("Unexpected FGHJ kind ").size());
    }
    bool seenFrame = false, seenValue = false, seenTension = false;
    for (const auto& [key, field] : data.members()) {
        bool* seen = key == "frame" ? &seenFrame : key == "value" ? &seenValue :
            key == "tension" ? &seenTension : nullptr;
        if (!seen) continue;
        if (*seen) return "ASDF deserialisation: multiple keys for member '" + key +
            "' in Keyframe are not allowed.";
        *seen = true;
        serde::ReadError error;
        if (key == "frame") error = serde::deserializeValue(field, frame);
        else if (key == "value") error = serde::deserializeValue(field, value);
        else error = serde::deserializeValue(field, tension);
        if (error) return error;
    }
    const char* missing = !seenFrame ? "frame" : !seenValue ? "value" : !seenTension ? "tension" : nullptr;
    if (missing) return std::string("FGHJ deserialisation: non-optional member '") + missing +
        "' in Keyframe is missing.";
    return {};
}
namespace {
std::string_view animationScopedEnumText(serde::TypedJsonView data) {
    if (data.kind() == serde::JsonKind::String) return data.stringBytes();
    if (data.kind() != serde::JsonKind::Null) (void)serde::unexpectedJsonKind(data);
    return {};
}
[[noreturn]] void animationEnumReadFailure(std::string_view text, std::string_view type) {
    std::string message = "Unable to deserialize string '";
    if (!text.empty()) message.append(text);
    message += "' to ";
    message.append(type);
    message += "Allowed keys:allowedKeys";
    throw d_runtime::ThrowableException(d_runtime::RetainedString(std::move(message)),
        d_runtime::RetainedString::fromStaticLiteral("fghj/serialization.d"), 2167);
}
void readInterpolation(serde::TypedJsonView data, common::InterpolateMode& mode) {
    const auto value = animationScopedEnumText(data);
    constexpr std::string_view keys[]{"Nearest", "Linear", "Stepped", "Cubic", "Bezier", "COUNT"};
    for (int index = 0; index < 6; ++index) {
        if (value == keys[index]) {
            mode = static_cast<common::InterpolateMode>(index);
            return;
        }
    }
    animationEnumReadFailure(value, "InterpolateMode");
}
void readMergeMode(serde::TypedJsonView data, param::ParamMergeMode& mode) {
    const auto value = animationScopedEnumText(data);
    constexpr std::string_view names[]{"Additive", "Weighted", "Multiplicative", "Forced", "Passthrough"};
    constexpr std::string_view aliases[]{"additive", "weighted", "multiplicative", "forced", "passthrough"};
    constexpr param::ParamMergeMode values[]{param::ParamMergeMode::Additive, param::ParamMergeMode::Weighted,
        param::ParamMergeMode::Multiplicative, param::ParamMergeMode::Forced, param::ParamMergeMode::Passthrough};
    for (int index = 0; index < 5; ++index) {
        if (value == names[index] || value == aliases[index]) {
            mode = values[index];
            return;
        }
    }
    animationEnumReadFailure(value, "ParamMergeMode");
}
}
serde::ReadError AnimationLane::deserializeFromFghj(serde::TypedJsonView data) {
    readInterpolation(data.findFirst("interpolation"), interpolation);
    (void)serde::deserializeValue(data.findFirst("uuid"), refuuid);
    paramRef = std::make_shared<AnimationParameterRef>();
    (void)serde::deserializeValue(data.findFirst("target"), paramRef->targetAxis);
    (void)serde::deserializeValue(data.findFirst("keyframes"), frames);
    const auto merge = data.findFirst("merge_mode");
    if (!merge.isMissing()) readMergeMode(merge, mergeMode);
    return {};
}
serde::ReadError Animation::deserializeFromFghj(serde::TypedJsonView data) {
    (void)serde::deserializeValue(data.findFirst("timestep"), timestep);
    (void)serde::deserializeValue(data.findFirst("additive"), additive);
    const auto weight = data.findFirst("animationWeight");
    if (!weight.isMissing()) (void)serde::deserializeValue(weight, animationWeight);
    (void)serde::deserializeValue(data.findFirst("length"), length);
    (void)serde::deserializeValue(data.findFirst("leadIn"), leadIn);
    (void)serde::deserializeValue(data.findFirst("leadOut"), leadOut);
    (void)serde::deserializeValue(data.findFirst("lanes"), lanes);
    return {};
}
}
