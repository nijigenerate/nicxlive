#pragma once

#include "commands.hpp"
#include "common.hpp"
#include "shared_deform_buffer.hpp"

#include <memory>
#include <vector>

namespace nicxlive::core::render {

using ::nicxlive::core::nodes::PartDrawPacket;

class QueueRenderBackend;

// D: nijilive.core.render.backends.opengl.queue.RenderQueue (リアルバックエンド向け)
class RenderQueue : public RenderCommandEmitter {
public:
    void beginFrame(RenderBackend* backend, RenderGpuState& state) override;
    void endFrame(RenderBackend*, RenderGpuState&) override;
    void beginMask(bool useStencil) override;
    void applyMask(const std::shared_ptr<::nicxlive::core::nodes::Drawable>& mask, bool dodge) override;
    void beginMaskContent() override;
    void endMask() override;
    void drawPartPacket(const PartDrawPacket& packet) override;
    void beginDynamicComposite(const std::shared_ptr<::nicxlive::core::nodes::Projectable>& composite, const DynamicCompositePass& pass) override;
    void endDynamicComposite(const std::shared_ptr<::nicxlive::core::nodes::Projectable>& composite, const DynamicCompositePass& pass) override;

private:
    bool ready() const;
    void uploadSharedBuffers();

    RenderBackend* activeBackend_{nullptr};
    RenderGpuState* frameState_{nullptr};
};

class QueueCommandEmitter : public RenderCommandEmitter {
public:
    explicit QueueCommandEmitter(const std::shared_ptr<QueueRenderBackend>& backend);

    void beginFrame(RenderBackend* backend, RenderGpuState& state) override;
    void endFrame(RenderBackend*, RenderGpuState&) override;
    void beginMask(bool useStencil) override;
    void applyMask(const std::shared_ptr<::nicxlive::core::nodes::Drawable>& mask, bool dodge) override;
    void beginMaskContent() override;
    void endMask() override;
    void drawPartPacket(const PartDrawPacket& packet) override;
    void beginDynamicComposite(const std::shared_ptr<::nicxlive::core::nodes::Projectable>& composite, const DynamicCompositePass& pass) override;
    void endDynamicComposite(const std::shared_ptr<::nicxlive::core::nodes::Projectable>& composite, const DynamicCompositePass& pass) override;
    void playback(RenderBackend* backend) override;

    const std::vector<QueuedCommand>& queue() const { return backendQueue(); }
    const std::vector<QueuedCommand>& recorded() const { return backendQueue(); }

private:
    const std::vector<QueuedCommand>& backendQueue() const;
    std::vector<QueuedCommand>& backendQueue();
    bool tryMakeMaskApplyPacket(const std::shared_ptr<::nicxlive::core::nodes::Drawable>& drawable, bool isDodge, RenderBackend::MaskApplyPacket& packet);
    void record(RenderCommandKind kind, const std::function<void(QueuedCommand&)>& fill);
    void uploadSharedBuffers();

    std::shared_ptr<QueueRenderBackend> backend_{};
    RenderBackend* activeBackend_{nullptr};
    bool pendingMask{false};
    bool pendingMaskUsesStencil{false};
};

} // namespace nicxlive::core::render
