#pragma once

#include "../common/shared_array.hpp"
#include "../param/parameter.hpp"
#include "../serde.hpp"
#include "../serde_reader.hpp"
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

namespace nicxlive::core {
class Puppet;
struct Keyframe {
    std::int32_t frame = 0;
    float value = std::numeric_limits<float>::quiet_NaN();
    float tension = 0.5f;
    void serialize(serde::TypedJsonWriter& serializer) const;
    serde::ReadError deserializeFromFghj(serde::TypedJsonView data);
};
struct AnimationParameterRef {
    std::shared_ptr<param::Parameter> targetParam;
    std::int32_t targetAxis = 0;
};
struct AnimationLane {
private:
    std::uint32_t refuuid = 0;
public:
    std::shared_ptr<AnimationParameterRef> paramRef;
    common::SharedArray<Keyframe> frames;
    common::InterpolateMode interpolation = common::InterpolateMode::Nearest;
    param::ParamMergeMode mergeMode = param::ParamMergeMode::Forced;
    AnimationLane() = default;
    AnimationLane(std::uint32_t referenceUuid, std::shared_ptr<AnimationParameterRef> reference,
                  common::SharedArray<Keyframe> keyframes,
                  common::InterpolateMode interpolationMode = common::InterpolateMode::Nearest,
                  param::ParamMergeMode parameterMergeMode = param::ParamMergeMode::Forced)
        : refuuid(referenceUuid), paramRef(std::move(reference)), frames(std::move(keyframes)),
          interpolation(interpolationMode), mergeMode(parameterMergeMode) {}
    void serialize(serde::TypedJsonWriter& serializer) const;
    serde::ReadError deserializeFromFghj(serde::TypedJsonView data);
    float get(float frame, bool snapSubframes = false) const;
    void reconstruct(const std::shared_ptr<Puppet>& puppet);
    void finalize(const std::shared_ptr<Puppet>& puppet);
    void updateFrames();
};
struct Animation {
    float timestep = 0.0166f;
    bool additive = false;
    float animationWeight = std::numeric_limits<float>::quiet_NaN();
    common::SharedArray<AnimationLane> lanes;
    std::int32_t length = 0;
    std::int32_t leadIn = -1;
    std::int32_t leadOut = -1;
    void reconstruct(const std::shared_ptr<Puppet>& puppet);
    void finalize(const std::shared_ptr<Puppet>& puppet);
    void serialize(serde::TypedJsonWriter& serializer) const;
    serde::ReadError deserializeFromFghj(serde::TypedJsonView data);
};
inline serde::ReadError deserializeValue(serde::TypedJsonView data, Keyframe& value) {
    return value.deserializeFromFghj(data);
}
inline serde::ReadError deserializeValue(serde::TypedJsonView data, AnimationLane& value) {
    return value.deserializeFromFghj(data);
}
inline serde::ReadError deserializeValue(serde::TypedJsonView data, Animation& value) {
    return value.deserializeFromFghj(data);
}
}
