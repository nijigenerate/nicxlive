#include "command_emitter.hpp"
#include "backend_queue.hpp"

#include <functional>

namespace nicxlive::core::render {

QueueCommandEmitter::QueueCommandEmitter(const std::shared_ptr<QueueRenderBackend>& backend) : backend_(backend) {}

void QueueCommandEmitter::beginFrame(RenderBackend* backend, RenderGpuState& state) {
    activeBackend_ = backend;
    state = RenderGpuState::init();
    pendingMask = false;
    pendingMaskUsesStencil = false;
    backendQueue().clear();
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
        record(RenderCommandKind::BeginMask, [&](QueuedCommand& cmd) { cmd.usesStencil = pendingMaskUsesStencil; });
        pendingMask = false;
    }
    record(RenderCommandKind::ApplyMask, [&](QueuedCommand& cmd) { cmd.maskApplyPacket = packet; });
}

void QueueCommandEmitter::beginMaskContent() {
    if (pendingMask) return; // ApplyMask が無効なら skip
    record(RenderCommandKind::BeginMaskContent, [](QueuedCommand&) {});
}

void QueueCommandEmitter::endMask() {
    if (pendingMask) {
        pendingMask = false;
        return;
    }
    record(RenderCommandKind::EndMask, [](QueuedCommand&) {});
    pendingMask = false;
    pendingMaskUsesStencil = false;
}

void QueueCommandEmitter::drawPartPacket(const nodes::PartDrawPacket& packet) {
    record(RenderCommandKind::DrawPart, [&](QueuedCommand& cmd) {
        cmd.partPacket = packet;
    });
}

void QueueCommandEmitter::beginDynamicComposite(const std::shared_ptr<nodes::Projectable>&, const DynamicCompositePass& pass) {
    record(RenderCommandKind::BeginDynamicComposite, [&](QueuedCommand& cmd) { cmd.dynamicPass = pass; });
}

void QueueCommandEmitter::endDynamicComposite(const std::shared_ptr<nodes::Projectable>&, const DynamicCompositePass& pass) {
    record(RenderCommandKind::EndDynamicComposite, [&](QueuedCommand& cmd) { cmd.dynamicPass = pass; });
}

void QueueCommandEmitter::playback(RenderBackend* backend) {
    if (!backend) return;
    for (const auto& cmd : backendQueue()) {
        switch (cmd.kind) {
        case RenderCommandKind::DrawPart:
            backend->drawPartPacket(cmd.partPacket);
            break;
        case RenderCommandKind::BeginDynamicComposite:
            backend->beginDynamicComposite(cmd.dynamicPass);
            break;
        case RenderCommandKind::EndDynamicComposite:
            backend->endDynamicComposite(cmd.dynamicPass);
            break;
        case RenderCommandKind::BeginMask:
            backend->beginMask(cmd.usesStencil);
            break;
        case RenderCommandKind::ApplyMask:
            backend->applyMask(cmd.maskApplyPacket);
            break;
        case RenderCommandKind::BeginMaskContent:
            backend->beginMaskContent();
            break;
        case RenderCommandKind::EndMask:
            backend->endMask();
            break;
        default:
            break;
        }
    }
}

const std::vector<QueuedCommand>& QueueCommandEmitter::backendQueue() const {
    static const std::vector<QueuedCommand> empty;
    return backend_ ? backend_->backendQueue() : empty;
}

std::vector<QueuedCommand>& QueueCommandEmitter::backendQueue() {
    static std::vector<QueuedCommand> dummy;
    return backend_ ? backend_->backendQueue() : dummy;
}

bool QueueCommandEmitter::tryMakeMaskApplyPacket(const std::shared_ptr<nodes::Drawable>& drawable, bool isDodge, RenderBackend::MaskApplyPacket& packet) {
    return ::nicxlive::core::render::tryMakeMaskApplyPacket(drawable, isDodge, packet);
}

void QueueCommandEmitter::record(RenderCommandKind kind, const std::function<void(QueuedCommand&)>& fill) {
    QueuedCommand cmd{};
    cmd.kind = kind;
    fill(cmd);
    backendQueue().push_back(std::move(cmd));
}

} // namespace nicxlive::core::render
