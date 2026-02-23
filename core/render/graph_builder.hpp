#pragma once

#include "common.hpp"
#include "render_pass.hpp"
#include "../nodes/projectable.hpp"

#include <functional>
#include <string>
#include <vector>

namespace nicxlive::core {

using RenderCommandBuilder = std::function<void(RenderCommandEmitter&)>;

struct RenderItem {
    float zSort{0.0f};
    std::size_t sequence{0};
    RenderCommandBuilder builder{};
};

struct RenderPass {
    RenderPassKind kind{RenderPassKind::Root};
    std::size_t token{0};
    float scopeZSort{0};
    std::weak_ptr<nodes::Projectable> projectable{};
    DynamicCompositePass dynamicPass{};
    std::vector<RenderItem> items{};
    std::size_t nextSequence{0};
    RenderCommandBuilder dynamicPostCommands{};
};

class RenderGraphBuilder {
public:
    RenderGraphBuilder();

    void beginFrame();
    void clear();
    bool empty() const;
    std::size_t rootItemCount() const;
    std::size_t passDepth() const;
    void enqueueItem(float zSort, RenderCommandBuilder builder);
    void enqueueItem(float zSort, const RenderScopeHint& hint, RenderCommandBuilder builder);
    std::size_t pushDynamicComposite(const std::shared_ptr<nodes::Projectable>& composite,
                                     const DynamicCompositePass& passData,
                                     float zSort);
    void popDynamicComposite(std::size_t token, RenderCommandBuilder postCommands = nullptr);
    void playback(RenderCommandEmitter* emitter);

private:
    std::vector<RenderPass> passStack_{};
    std::size_t nextToken_{0};

    void ensureRootPass();
    static bool itemLess(const RenderItem& a, const RenderItem& b);
    std::vector<RenderItem> collectPassItems(RenderPass& pass);
    static void playbackItems(const std::vector<RenderItem>& items, RenderCommandEmitter& emitter);
    void addItemToPass(RenderPass& pass, float zSort, RenderCommandBuilder builder);
    void finalizeDynamicCompositePass(bool autoClose, RenderCommandBuilder postCommands = nullptr);
    void finalizeTopPass(bool autoClose);
    RenderPass& resolvePass(const RenderScopeHint& hint);
    std::size_t findPassIndex(std::size_t token, RenderPassKind kind) const;
    std::size_t parentPassIndexForDynamic(const std::shared_ptr<nodes::Projectable>& composite) const;
    std::string stackDebugString() const;
};

} // namespace nicxlive::core
