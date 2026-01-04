#pragma once

#include "commands.hpp"
#include "common.hpp"

#include <memory>
#include <vector>

namespace nicxlive::core::render {

class QueueRenderBackend;

class QueueCommandEmitter : public RenderCommandEmitter {
public:
    explicit QueueCommandEmitter(const std::shared_ptr<QueueRenderBackend>& backend);

    void beginFrame(RenderBackend* backend, RenderGpuState& state) override;
    void endFrame(RenderBackend*, RenderGpuState&) override;
    void beginMask(bool useStencil) override;
    void applyMask(const std::shared_ptr<nodes::Drawable>& mask, bool dodge) override;
    void beginMaskContent() override;
    void endMask() override;
    void drawPartPacket(const nodes::PartDrawPacket& packet) override;
    void beginDynamicComposite(const std::shared_ptr<nodes::Projectable>& composite, const DynamicCompositePass& pass) override;
    void endDynamicComposite(const std::shared_ptr<nodes::Projectable>& composite, const DynamicCompositePass& pass) override;
    void playback(RenderBackend* backend) override;

    const std::vector<QueuedCommand>& queue() const { return backendQueue(); }
    const std::vector<QueuedCommand>& recorded() const { return backendQueue(); }

private:
    const std::vector<QueuedCommand>& backendQueue() const;
    std::vector<QueuedCommand>& backendQueue();
    bool tryMakeMaskApplyPacket(const std::shared_ptr<nodes::Drawable>& drawable, bool isDodge, RenderBackend::MaskApplyPacket& packet);
    void record(RenderCommandKind kind, const std::function<void(QueuedCommand&)>& fill);

    std::shared_ptr<QueueRenderBackend> backend_{};
    RenderBackend* activeBackend_{nullptr};
    bool pendingMask{false};
    bool pendingMaskUsesStencil{false};
};

} // namespace nicxlive::core::render
