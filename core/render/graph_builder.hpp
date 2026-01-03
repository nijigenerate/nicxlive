#pragma once

#include "common.hpp"

#include <functional>
#include <memory>
#include <vector>

namespace nicxlive::core {

class RenderGraphBuilder {
public:
    bool empty() const { return tasks_.empty(); }
    void beginFrame() {}
    void playback(RenderCommandEmitter* emitter) {
        if (!emitter) return;
        for (auto& fn : tasks_) {
            fn(*emitter);
        }
    }
    void addTask(std::function<void(RenderCommandEmitter&)> fn) {
        tasks_.push_back(std::move(fn));
    }

    std::size_t pushDynamicComposite(const std::shared_ptr<nodes::Projectable>& /*node*/,
                                     const DynamicCompositePass& /*pass*/,
                                     float /*zSort*/) {
        return ++tokenCounter_;
    }

    void popDynamicComposite(std::size_t /*token*/, std::function<void(RenderCommandEmitter&)> fn) {
        addTask(std::move(fn));
    }

    void applyMask(const std::shared_ptr<nodes::Drawable>& mask, bool dodge) {
        addTask([mask, dodge](RenderCommandEmitter& emitter) { emitter.applyMask(mask, dodge); });
    }

    void drawPart(const std::shared_ptr<nodes::Part>& part, bool isMask) {
        addTask([part, isMask](RenderCommandEmitter& emitter) { emitter.drawPart(part, isMask); });
    }

private:
    std::vector<std::function<void(RenderCommandEmitter&)>> tasks_{};
    std::size_t tokenCounter_{0};
};

} // namespace nicxlive::core
