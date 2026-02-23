#include "graph_builder.hpp"
#include "../nodes/projectable.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>

namespace nicxlive::core {
namespace {
int gGraphDebugLogs = 0;
int gGraphAddLogs = 0;
}

RenderGraphBuilder::RenderGraphBuilder() { ensureRootPass(); }

void RenderGraphBuilder::beginFrame() {
    nodes::advanceProjectableFrame();
    clear();
}

void RenderGraphBuilder::clear() {
    passStack_.clear();
    nextToken_ = 0;
    ensureRootPass();
}

bool RenderGraphBuilder::empty() const {
    if (passStack_.empty()) return true;
    if (passStack_.size() == 1) return passStack_.front().items.empty();
    return false;
}

std::size_t RenderGraphBuilder::rootItemCount() const {
    if (passStack_.empty()) return 0;
    return passStack_.front().items.size();
}

std::size_t RenderGraphBuilder::passDepth() const {
    return passStack_.size();
}

void RenderGraphBuilder::enqueueItem(float zSort, RenderCommandBuilder builder) {
    enqueueItem(zSort, RenderScopeHint::root(), std::move(builder));
}

void RenderGraphBuilder::enqueueItem(float zSort, const RenderScopeHint& hint, RenderCommandBuilder builder) {
    if (!builder || hint.skip) return;
    auto& pass = resolvePass(hint);
    addItemToPass(pass, zSort, std::move(builder));
}

std::size_t RenderGraphBuilder::pushDynamicComposite(const std::shared_ptr<nodes::Projectable>& composite,
                                                     const DynamicCompositePass& passData,
                                                     float zSort) {
    ensureRootPass();
    RenderPass pass;
    pass.kind = RenderPassKind::DynamicComposite;
    pass.projectable = composite;
    pass.dynamicPass = passData;
    pass.scopeZSort = zSort;
    pass.token = ++nextToken_;
    pass.nextSequence = 0;
    pass.dynamicPostCommands = nullptr;
    passStack_.push_back(pass);
    return pass.token;
}

void RenderGraphBuilder::popDynamicComposite(std::size_t token, RenderCommandBuilder postCommands) {
    if (passStack_.size() <= 1) throw std::runtime_error("RenderGraphBuilder.popDynamicComposite without matching push. " + stackDebugString());
    std::size_t targetIndex = findPassIndex(token, RenderPassKind::DynamicComposite);
    if (targetIndex == 0) throw std::runtime_error("RenderGraphBuilder.popDynamicComposite scope mismatch token=" + std::to_string(token) + " " + stackDebugString());
    if (postCommands) passStack_[targetIndex].dynamicPostCommands = postCommands;
    while (passStack_.size() - 1 > targetIndex) finalizeTopPass(true);
    auto& pass = passStack_.back();
    if (pass.token != token) throw std::runtime_error("RenderGraphBuilder.popDynamicComposite token mismatch. " + stackDebugString());
    finalizeDynamicCompositePass(false, postCommands);
}

void RenderGraphBuilder::playback(RenderCommandEmitter* emitter) {
    if (!emitter) return;
    if (passStack_.size() != 1) throw std::runtime_error("RenderGraphBuilder scopes not balanced before playback. " + stackDebugString());
    auto rootItems = collectPassItems(passStack_.front());
    clear();
    playbackItems(rootItems, *emitter);
}

void RenderGraphBuilder::ensureRootPass() {
    if (!passStack_.empty()) return;
    RenderPass root;
    root.kind = RenderPassKind::Root;
    root.token = 0;
    root.scopeZSort = 0;
    root.nextSequence = 0;
    passStack_.push_back(root);
}

bool RenderGraphBuilder::itemLess(const RenderItem& a, const RenderItem& b) {
    if (a.zSort == b.zSort) return a.sequence < b.sequence;
    return a.zSort > b.zSort;
}

std::vector<RenderItem> RenderGraphBuilder::collectPassItems(RenderPass& pass) {
    if (pass.items.empty()) return {};
    std::sort(pass.items.begin(), pass.items.end(), itemLess);
    return pass.items;
}

void RenderGraphBuilder::playbackItems(const std::vector<RenderItem>& items, RenderCommandEmitter& emitter) {
    for (const auto& item : items) {
        if (item.builder) item.builder(emitter);
    }
}

void RenderGraphBuilder::addItemToPass(RenderPass& pass, float zSort, RenderCommandBuilder builder) {
    if (!builder) return;
    RenderItem item;
    item.zSort = zSort;
    item.sequence = pass.nextSequence++;
    item.builder = std::move(builder);
    pass.items.push_back(item);
    if (gGraphAddLogs < 64 && pass.kind == RenderPassKind::Root && zSort < -0.6f && zSort > -0.95f) {
        std::fprintf(stderr, "[nicxlive] graph add root z=%.6f seq=%zu passToken=%zu kind=%d\n",
                     zSort, item.sequence, pass.token, static_cast<int>(pass.kind));
        ++gGraphAddLogs;
    }
}

void RenderGraphBuilder::finalizeDynamicCompositePass(bool autoClose, RenderCommandBuilder postCommands) {
    if (passStack_.size() <= 1) throw std::runtime_error("RenderGraphBuilder: cannot finalize dynamic composite scope without active pass. " + stackDebugString());
    RenderPass pass = passStack_.back();
    if (pass.kind != RenderPassKind::DynamicComposite) throw std::runtime_error("RenderGraphBuilder: top scope is not dynamic composite. " + stackDebugString());
    std::size_t parentIndex = parentPassIndexForDynamic(pass.projectable.lock());
    passStack_.pop_back();

    auto childItems = collectPassItems(pass);

    if (!pass.dynamicPass.surface) {
        if (autoClose) {
            if (auto p = pass.projectable.lock()) {
                p->dynamicScopeActive = false;
                p->dynamicScopeToken = std::numeric_limits<std::size_t>::max();
            }
        }
        pass.dynamicPostCommands = nullptr;
        return;
    }

    auto dynamicNode = pass.projectable.lock();
    auto passData = pass.dynamicPass;
    auto finalizer = postCommands ? postCommands : pass.dynamicPostCommands;
    std::fprintf(stderr, "[nicxlive] graph finalize dyn token=%llu z=%.6f autoClose=%d hasFinalizer=%d childItems=%llu stencil=%u\n",
                 static_cast<unsigned long long>(pass.token),
                 pass.scopeZSort,
                 autoClose ? 1 : 0,
                 finalizer ? 1 : 0,
                 static_cast<unsigned long long>(childItems.size()),
                 passData.stencil ? passData.stencil->backendId() : 0u);
    RenderCommandBuilder builder = [childItems, dynamicNode, passData, finalizer](RenderCommandEmitter& emitter) {
        emitter.beginDynamicComposite(dynamicNode, passData);
        playbackItems(childItems, emitter);
        emitter.endDynamicComposite(dynamicNode, passData);
        if (finalizer) finalizer(emitter);
    };

    addItemToPass(passStack_[parentIndex], pass.scopeZSort, std::move(builder));

    if (autoClose && dynamicNode) {
        dynamicNode->dynamicScopeActive = false;
        dynamicNode->dynamicScopeToken = std::numeric_limits<std::size_t>::max();
    }
    pass.dynamicPostCommands = nullptr;
}

void RenderGraphBuilder::finalizeTopPass(bool autoClose) {
    if (passStack_.size() <= 1) throw std::runtime_error("RenderGraph: no scope available to finalize. " + stackDebugString());
    auto kind = passStack_.back().kind;
    switch (kind) {
    case RenderPassKind::DynamicComposite:
        finalizeDynamicCompositePass(autoClose, nullptr);
        break;
    case RenderPassKind::Root:
        throw std::runtime_error("RenderGraph: cannot finalize root pass.");
    }
}

RenderPass& RenderGraphBuilder::resolvePass(const RenderScopeHint& hint) {
    ensureRootPass();
    std::size_t index = 0;
    switch (hint.kind) {
    case RenderPassKind::Root:
        index = 0;
        break;
    case RenderPassKind::DynamicComposite:
        index = findPassIndex(hint.token, RenderPassKind::DynamicComposite);
        if (index == 0) throw std::runtime_error("RenderGraph: dynamic composite scope not active for enqueue. " + stackDebugString());
        break;
    }
    return passStack_[index];
}

std::size_t RenderGraphBuilder::findPassIndex(std::size_t token, RenderPassKind kind) const {
    if (passStack_.size() <= 1) return 0;
    for (std::size_t idx = passStack_.size(); idx > 0; --idx) {
        const auto& pass = passStack_[idx - 1];
        if (pass.token == token && pass.kind == kind) return idx - 1;
    }
    return 0;
}

std::size_t RenderGraphBuilder::parentPassIndexForDynamic(const std::shared_ptr<nodes::Projectable>& composite) const {
    if (!composite || passStack_.empty()) return 0;
    auto ancestor = composite->parentPtr();
    while (ancestor) {
        if (auto proj = std::dynamic_pointer_cast<nodes::Projectable>(ancestor)) {
            auto token = proj->dynamicScopeTokenValue();
            auto idx = findPassIndex(token, RenderPassKind::DynamicComposite);
            if (idx > 0) return idx;
        }
        ancestor = ancestor->parentPtr();
    }
    return 0;
}

std::string RenderGraphBuilder::stackDebugString() const {
    std::string buf = "[stack depth=" + std::to_string(passStack_.size()) + "]";
    for (std::size_t i = 0; i < passStack_.size(); ++i) {
        buf += " #" + std::to_string(i) + "(kind=" + std::to_string(static_cast<int>(passStack_[i].kind)) +
               ",token=" + std::to_string(passStack_[i].token) + ")";
    }
    return buf;
}

} // namespace nicxlive::core
