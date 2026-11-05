#pragma once
// Independent nijilive/core/animation/player.d port. BSD-2-Clause.
#include "animation.hpp"
#include "../compat/retained_string.hpp"
#include <common/shared_array.hpp>
#include <memory>
#include <concepts>
#include <string>
#include <cstddef>

namespace nicxlive::core { class Puppet; }
namespace nicxlive::core {
using AnimationName = d_runtime::RetainedString;
struct AnimationPlayback;
struct AnimationArena;
using AnimationPlaybackRef = std::shared_ptr<AnimationPlayback>;
class AnimationPlayer {
    friend struct AnimationPlayback;
    friend struct AnimationArena;
    std::weak_ptr<AnimationArena> arena;
    std::shared_ptr<nicxlive::core::Puppet> puppet;
    nicxlive::core::common::SharedArray<AnimationPlayback*> playingAnimations;
    static std::shared_ptr<AnimationPlayer> adoptOwnedPlayer(std::unique_ptr<AnimationPlayer> player);
public:
    explicit AnimationPlayer(std::shared_ptr<nicxlive::core::Puppet> value);
    virtual ~AnimationPlayer() = default;
    AnimationPlayer(const AnimationPlayer&) = delete;
    AnimationPlayer& operator=(const AnimationPlayer&) = delete;
    AnimationPlayer(AnimationPlayer&&) = delete;
    AnimationPlayer& operator=(AnimationPlayer&&) = delete;
    bool snapToFramerate = false;
    static std::shared_ptr<AnimationPlayer> create(std::shared_ptr<nicxlive::core::Puppet> value);
    template<class T, class... Args> requires std::derived_from<T, AnimationPlayer>
    static std::shared_ptr<T> createDerived(Args&&... args) {
        auto player = std::make_unique<T>(std::forward<Args>(args)...);
        auto* instance = player.get();
        auto owner = adoptOwnedPlayer(std::move(player));
        return std::shared_ptr<T>(owner, instance);
    }
    virtual void update(float delta);
    virtual AnimationPlaybackRef createOrGet(AnimationName name);
    virtual AnimationPlaybackRef play(AnimationName name);
    virtual void prerenderAll();
    virtual void stopAll(bool immediate = false);
    virtual void destroyAll();
};
struct AnimationPlayback {
private:
    friend class AnimationPlayer;
    AnimationPlayer* player{};
    nicxlive::core::Animation* anim{};
    std::shared_ptr<nicxlive::core::Animation> animationOwner;
    std::shared_ptr<AnimationArena> retainedPlayer_;
    AnimationArena* containingArena_{};
    std::shared_ptr<AnimationArena> retainPlayer() const;
    void copyValue(const AnimationPlayback& other);
    bool valid = true;
    AnimationName _name;
    bool _playLeadOut = false, _paused = false, _playing = false;
    bool _looping = false, _stopping = false;
    float _time = 0, _strength = 1, _speed = 1;
    int _looped = 0;
    std::shared_ptr<nicxlive::core::Puppet>& getPuppet();
    void update(float delta);
public:
    AnimationPlayback() = default;
    AnimationPlayback(AnimationPlayer* player, nicxlive::core::Animation* anim, AnimationName name);
    AnimationPlayback(const AnimationPlayback& other);
    AnimationPlayback(AnimationPlayback&& other);
    AnimationPlayback& operator=(const AnimationPlayback& other);
    AnimationPlayback& operator=(AnimationPlayback&& other);
    AnimationName name() const;
    bool eof() const;
    bool isValid() const;
    bool playing() const;
    bool stopping() const;
    bool paused() const;
    bool looping() const;
    bool looping(bool value);
    int looped() const;
    float speed() const;
    float speed(bool value);
    float strength() const;
    float strength(float value);
    int frame() const;
    float hframe() const;
    int loopPointEnd() const;
    int loopPointBegin() const;
    bool hasLeadIn() const;
    bool hasLeadOut() const;
    bool isPlayingLeadOut() const;
    bool isRunning() const;
    int fps() const;
    int seconds() const;
    int miliseconds() const;
    int frames() const;
    nicxlive::core::Animation* animation() const;
    std::ptrdiff_t playbackId() const;
    void destroy();
    void play(bool loop = false, bool playLeadOut = true);
    void pause();
    void stop(bool immediate = false);
    void seek(int frame);
    void render();
};
}
