#pragma once

#include <string>
#include <chrono>

namespace nicxlive::core::render {

struct RenderProfileScope {
    explicit RenderProfileScope(const std::string& label);
    RenderProfileScope(const RenderProfileScope&) = delete;
    RenderProfileScope& operator=(const RenderProfileScope&) = delete;
    RenderProfileScope(RenderProfileScope&& other) noexcept;
    RenderProfileScope& operator=(RenderProfileScope&& other) noexcept;
    ~RenderProfileScope();
    void stop();

private:
    std::string label_;
    bool active_{false};
    std::chrono::steady_clock::time_point start_;
};

RenderProfileScope profileScope(const std::string& label);
void renderProfilerFrameCompleted();

} // namespace nicxlive::core::render
