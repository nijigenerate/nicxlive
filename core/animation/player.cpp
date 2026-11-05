// Independent nijilive/core/animation/player.d port. BSD-2-Clause.
#include "player.hpp"
#include "../compat/native_access_fault.hpp"
#include <puppet.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace nicxlive::core {
// External Player/Playback handles alias one arena. Internal back-references
// borrow their own containing graph; detached values retain their real player.
// Removed playbacks stay address-stable for retained external references.
struct AnimationArena {
    std::unique_ptr<AnimationPlayer> player;
    std::vector<std::unique_ptr<AnimationPlayback>> playbacks;
    explicit AnimationArena(std::unique_ptr<AnimationPlayer> value)
        : player(std::move(value)) {}
};
namespace {
int asInt(double value) {
    if (!std::isfinite(value) || value < -2147483648.0 || value >= 2147483648.0)
        return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(value);
}
int next(int value) { return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value) + 1u); }
int previous(int value) { return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value) - 1u); }
}
AnimationPlayer::AnimationPlayer(std::shared_ptr<nicxlive::core::Puppet> value) : puppet(std::move(value)) {}
std::shared_ptr<AnimationPlayer> AnimationPlayer::create(std::shared_ptr<nicxlive::core::Puppet> value) {
    return adoptOwnedPlayer(std::make_unique<AnimationPlayer>(std::move(value)));
}
std::shared_ptr<AnimationPlayer> AnimationPlayer::adoptOwnedPlayer(std::unique_ptr<AnimationPlayer> player) {
    auto owner = std::make_shared<AnimationArena>(std::move(player));
    owner->player->arena = owner;
    return std::shared_ptr<AnimationPlayer>(owner, owner->player.get());
}
void AnimationPlayer::update(float delta) {
    [[maybe_unused]] const auto receiverLifetime = arena.lock();
    const auto active = playingAnimations;
    for (auto& animation : active) if (animation->valid) animation->update(delta);
}
AnimationPlaybackRef AnimationPlayer::createOrGet(AnimationName name) {
    auto owner = arena.lock();
    const auto active = playingAnimations;
    for (auto* animation : active)
        if (animation->_name == name) return AnimationPlaybackRef(owner, animation);
    auto& animations = d_runtime::ngRequireObject(puppet)->getAnimations();
    if (auto found = animations.find(name); found != animations.end()) {
        auto animation = std::make_unique<AnimationPlayback>(this, &found->second, std::move(name));
        animation->animationOwner = animations.retain(found);
        animation->containingArena_ = owner.get();
        auto* result = animation.get();
        owner->playbacks.push_back(std::move(animation));
        playingAnimations.push_back(result);
        return AnimationPlaybackRef(owner, result);
    }
    return {};
}
AnimationPlaybackRef AnimationPlayer::play(AnimationName name) {
    [[maybe_unused]] const auto receiverLifetime = arena.lock();
    auto animation = createOrGet(std::move(name));
    if (animation) animation->play();
    return animation;
}
void AnimationPlayer::prerenderAll() {
    [[maybe_unused]] const auto receiverLifetime = arena.lock();
    const auto active = playingAnimations;
    for (auto animation : active) animation->render();
}
void AnimationPlayer::stopAll(bool immediate) {
    [[maybe_unused]] const auto receiverLifetime = arena.lock();
    const auto active = playingAnimations;
    for (auto animation : active) animation->stop(immediate);
}
void AnimationPlayer::destroyAll() {
    [[maybe_unused]] const auto receiverLifetime = arena.lock();
    const auto active = playingAnimations;
    for (auto animation : active) animation->valid = false;
    playingAnimations.clear();
}
AnimationPlayback::AnimationPlayback(AnimationPlayer* value,
        nicxlive::core::Animation* animation, AnimationName name)
    : player(value), anim(animation), _name(std::move(name)) {}
std::shared_ptr<AnimationArena> AnimationPlayback::retainPlayer() const {
    if (retainedPlayer_) return retainedPlayer_;
    return player ? player->arena.lock() : nullptr;
}
AnimationPlayback::AnimationPlayback(const AnimationPlayback& other)
    : player(other.player), anim(other.anim) {
    copyValue(other);
}
AnimationPlayback::AnimationPlayback(AnimationPlayback&& other)
    : AnimationPlayback(static_cast<const AnimationPlayback&>(other)) {}
void AnimationPlayback::copyValue(const AnimationPlayback& other) {
    // Retain both actual owners before changing any pointer or releasing the
    // previous value. Embedded records borrow only their own containing graph.
    [[maybe_unused]] const auto previousPlayer = retainPlayer();
    auto nextPlayer = other.retainPlayer();
    const auto nextAnimation = other.animationOwner;
    const auto nextName = other._name;
    player = other.player;
    anim = other.anim;
    animationOwner = nextAnimation;
    valid = other.valid;
    _name = nextName;
    _playLeadOut = other._playLeadOut;
    _paused = other._paused;
    _playing = other._playing;
    _looping = other._looping;
    _stopping = other._stopping;
    _time = other._time;
    _strength = other._strength;
    _speed = other._speed;
    _looped = other._looped;
    if (nextPlayer.get() == containingArena_) nextPlayer.reset();
    retainedPlayer_ = std::move(nextPlayer);
}
AnimationPlayback& AnimationPlayback::operator=(const AnimationPlayback& other) {
    if (this != &other) copyValue(other);
    return *this;
}
AnimationPlayback& AnimationPlayback::operator=(AnimationPlayback&& other) {
    return operator=(static_cast<const AnimationPlayback&>(other));
}
std::shared_ptr<nicxlive::core::Puppet>& AnimationPlayback::getPuppet() { return d_runtime::ngRequireObject(player)->puppet; }
void AnimationPlayback::update(float delta) {
    if (!valid || !isRunning()) return;
    if (_paused) { render(); return; }
    _time += delta;
    if (!isPlayingLeadOut() && looping() && frame() >= loopPointEnd()) {
        _time = static_cast<float>(loopPointBegin()) * d_runtime::ngRequireObject(anim)->timestep;
        _looped = next(_looped);
    }
    if (next(frame()) >= frames()) _time = static_cast<float>(previous(frames())) * d_runtime::ngRequireObject(anim)->timestep;
    render();
    if (!_looping && isPlayingLeadOut() && next(frame()) >= d_runtime::ngRequireObject(anim)->length) {
        _playing = false; _playLeadOut = false; _stopping = false; _time = 0; _looped = 0;
    }
}
AnimationName AnimationPlayback::name() const { return _name; }
bool AnimationPlayback::eof() const { return frame() >= d_runtime::ngRequireObject(anim)->length; }
bool AnimationPlayback::isValid() const { return valid; }
bool AnimationPlayback::playing() const { return _playing; }
bool AnimationPlayback::stopping() const { return _stopping; }
bool AnimationPlayback::paused() const { return _paused; }
bool AnimationPlayback::looping() const { return _looping; }
bool AnimationPlayback::looping(bool value) { _looping = value; return value; }
int AnimationPlayback::looped() const { return _looped; }
float AnimationPlayback::speed() const { return _speed; }
float AnimationPlayback::speed(bool value) { _speed = std::clamp(static_cast<int>(value), 1, 10); return value; }
float AnimationPlayback::strength() const { return _strength; }
float AnimationPlayback::strength(float value) { _strength = std::clamp(value, 0.0f, 1.0f); return value; }
int AnimationPlayback::frame() const { return asInt(std::round(_time / d_runtime::ngRequireObject(anim)->timestep)); }
float AnimationPlayback::hframe() const { return _time / d_runtime::ngRequireObject(anim)->timestep; }
int AnimationPlayback::loopPointEnd() const { return hasLeadOut() ? d_runtime::ngRequireObject(anim)->leadOut : d_runtime::ngRequireObject(anim)->length; }
int AnimationPlayback::loopPointBegin() const { return hasLeadIn() ? d_runtime::ngRequireObject(anim)->leadIn : 0; }
bool AnimationPlayback::hasLeadIn() const { return d_runtime::ngRequireObject(anim)->leadIn > 0 && next(d_runtime::ngRequireObject(anim)->leadIn) < d_runtime::ngRequireObject(anim)->length; }
bool AnimationPlayback::hasLeadOut() const { return d_runtime::ngRequireObject(anim)->leadOut > 0 && next(d_runtime::ngRequireObject(anim)->leadOut) < d_runtime::ngRequireObject(anim)->length; }
bool AnimationPlayback::isPlayingLeadOut() const {
    return ((_playing && !_looping) || _stopping) && _playLeadOut && frame() < d_runtime::ngRequireObject(anim)->length;
}
bool AnimationPlayback::isRunning() const { return _playing || isPlayingLeadOut(); }
int AnimationPlayback::fps() const { return asInt(1000.0 / (d_runtime::ngRequireObject(anim)->timestep * 1000.0)); }
int AnimationPlayback::seconds() const { return asInt(_time); }
int AnimationPlayback::miliseconds() const { return asInt((_time - static_cast<float>(seconds())) * 1000); }
int AnimationPlayback::frames() const { return d_runtime::ngRequireObject(anim)->length; }
nicxlive::core::Animation* AnimationPlayback::animation() const { return anim; }
std::ptrdiff_t AnimationPlayback::playbackId() const {
    std::ptrdiff_t result = -1;
    const auto active = d_runtime::ngRequireObject(player)->playingAnimations;
    for (std::size_t i = 0; i < active.size(); ++i)
        if (active[i]->_name == _name) result = static_cast<std::ptrdiff_t>(i);
    return result;
}
void AnimationPlayback::destroy() {
    valid = false;
    if (playbackId() > -1) {
        const auto active = d_runtime::ngRequireObject(player)->playingAnimations;
        const auto removed = active.removed(static_cast<std::size_t>(playbackId()));
        d_runtime::ngRequireObject(player)->playingAnimations = removed;
    }
}
void AnimationPlayback::play(bool loop, bool playLeadOut) {
    if (_paused) _paused = false;
    else {
        _looped = 0; _time = 0; _stopping = false; _playing = true;
        _looping = loop; _playLeadOut = playLeadOut;
    }
}
void AnimationPlayback::pause() { _paused = true; }
void AnimationPlayback::stop(bool immediate) {
    if (_stopping) return;
    const bool stopNow = immediate || frame() == 0 || _paused || !hasLeadOut();
    _stopping = !stopNow; _looping = false; _paused = false; _playing = false; _playLeadOut = !stopNow;
    if (stopNow) { _time = 0; _looped = 0; }
}
void AnimationPlayback::seek(int frame) {
    const float frameTime = static_cast<float>(std::clamp(frame, 0, frames()));
    _time = frameTime * d_runtime::ngRequireObject(anim)->timestep; _looped = 0;
}
void AnimationPlayback::render() {
    [[maybe_unused]] const auto receiverLifetime = player
        ? player->arena.lock() : std::shared_ptr<AnimationArena>{};
    const float realStrength = std::clamp(_strength, 0.0f, 1.0f);
    const auto lanes = d_runtime::ngRequireObject(anim)->lanes;
    for (auto lane : lanes) {
        const auto target = d_runtime::ngRequireObject(
            d_runtime::ngRequireObject(lane.paramRef)->targetParam);
        const auto axis = d_runtime::ngRequireObject(lane.paramRef)->targetAxis;
        const auto frame = hframe();
        const auto snap = d_runtime::ngRequireObject(player)->snapToFramerate;
        const auto value = lane.get(frame, snap) * realStrength;
        const auto mergeMode = lane.mergeMode;
        target->pushIOffsetAxis(axis, value, mergeMode);
    }
}
}
