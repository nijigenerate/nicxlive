#include "command_emitter.hpp"
#include "backend_queue.hpp"

namespace nicxlive::core::render {

namespace nodes = ::nicxlive::core::nodes;

// --- QueueCommandEmitter (キュー収集用) ---

QueueCommandEmitter::QueueCommandEmitter(const std::shared_ptr<QueueRenderBackend>& backend) : backend_(backend) {}

void QueueCommandEmitter::beginFrame(RenderBackend* backend, RenderGpuState& state) {
    activeBackend_ = backend;
    state = RenderGpuState::init();
    pendingMask = false;
    pendingMaskUsesStencil = false;
    if (backend_) {
        backend_->queue.clear();
    }
    uploadSharedBuffers();
}

void QueueCommandEmitter::endFrame(RenderBackend*, RenderGpuState&) {
    activeBackend_ = nullptr;
    pendingMask = false;
    pendingMaskUsesStencil = false;
}

void QueueCommandEmitter::beginMask(bool useStencil) {
    // D 版では BeginMask は ApplyMask が成立したときに初めてキューへ積む
    pendingMask = true;
    pendingMaskUsesStencil = useStencil;
}

void QueueCommandEmitter::applyMask(const std::shared_ptr<nodes::Drawable>& mask, bool dodge) {
    if (!mask) return;
    RenderBackend::MaskApplyPacket packet{};
    if (!tryMakeMaskApplyPacket(mask, dodge, packet)) {
        // ApplyMask に失敗した場合は pendingMask を破棄
        pendingMask = false;
        return;
    }
    if (pendingMask) {
        QueuedCommand cmd{};
        cmd.kind = RenderCommandKind::BeginMask;
        cmd.usesStencil = pendingMaskUsesStencil;
        if (backend_) backend_->queue.push_back(std::move(cmd));
        pendingMask = false;
    }
    QueuedCommand cmd{};
    cmd.kind = RenderCommandKind::ApplyMask;
    cmd.maskApplyPacket = packet;
    if (backend_) backend_->queue.push_back(std::move(cmd));
}

void QueueCommandEmitter::beginMaskContent() {
    if (pendingMask) {
        // D版: BeginMask は ApplyMask が成立したときのみキューへ積む。
        // ApplyMask が来ずに BeginMaskContent だけ来た場合は何もせず破棄。
        return;
    }
    QueuedCommand cmd{};
    cmd.kind = RenderCommandKind::BeginMaskContent;
    if (backend_) backend_->queue.push_back(std::move(cmd));
}

void QueueCommandEmitter::endMask() {
    if (pendingMask) {
        pendingMask = false;
        return;
    }
    QueuedCommand cmd{};
    cmd.kind = RenderCommandKind::EndMask;
    if (backend_) backend_->queue.push_back(std::move(cmd));
    pendingMask = false;
    pendingMaskUsesStencil = false;
}

void QueueCommandEmitter::drawPartPacket(const nodes::PartDrawPacket& packet) {
    QueuedCommand cmd{};
    cmd.kind = RenderCommandKind::DrawPart;
    cmd.partPacket = packet;
    if (backend_) backend_->queue.push_back(std::move(cmd));
}

void QueueCommandEmitter::beginDynamicComposite(const std::shared_ptr<nodes::Projectable>&, const DynamicCompositePass& pass) {
    QueuedCommand cmd{};
    cmd.kind = RenderCommandKind::BeginDynamicComposite;
    cmd.dynamicPass = pass;
    if (backend_) backend_->queue.push_back(std::move(cmd));
}

void QueueCommandEmitter::endDynamicComposite(const std::shared_ptr<nodes::Projectable>&, const DynamicCompositePass& pass) {
    QueuedCommand cmd{};
    cmd.kind = RenderCommandKind::EndDynamicComposite;
    cmd.dynamicPass = pass;
    if (backend_) backend_->queue.push_back(std::move(cmd));
}

bool QueueCommandEmitter::tryMakeMaskApplyPacket(const std::shared_ptr<nodes::Drawable>& drawable, bool isDodge, RenderBackend::MaskApplyPacket& packet) {
    return ::nicxlive::core::render::tryMakeMaskApplyPacket(drawable, isDodge, packet);
}

void QueueCommandEmitter::uploadSharedBuffers() {
    if (!activeBackend_) return;
    using ::nicxlive::core::render::sharedDeformBufferData;
    using ::nicxlive::core::render::sharedDeformBufferDirty;
    using ::nicxlive::core::render::sharedDeformMarkUploaded;
    using ::nicxlive::core::render::sharedVertexBufferData;
    using ::nicxlive::core::render::sharedVertexBufferDirty;
    using ::nicxlive::core::render::sharedVertexMarkUploaded;
    using ::nicxlive::core::render::sharedUvBufferData;
    using ::nicxlive::core::render::sharedUvBufferDirty;
    using ::nicxlive::core::render::sharedUvMarkUploaded;

    if (sharedVertexBufferDirty()) {
        activeBackend_->uploadSharedVertexBuffer(sharedVertexBufferData());
        sharedVertexMarkUploaded();
    }
    if (sharedUvBufferDirty()) {
        activeBackend_->uploadSharedUvBuffer(sharedUvBufferData());
        sharedUvMarkUploaded();
    }
    if (sharedDeformBufferDirty()) {
        activeBackend_->uploadSharedDeformBuffer(sharedDeformBufferData());
        sharedDeformMarkUploaded();
    }
}

} // namespace nicxlive::core::render
