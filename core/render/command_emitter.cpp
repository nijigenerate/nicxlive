#include "command_emitter.hpp"
#include "backend_queue.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

namespace nicxlive::core::render {

namespace nodes = ::nicxlive::core::nodes;

namespace {
bool traceSharedEnabled() {
    static int enabled = -1;
    if (enabled >= 0) return enabled != 0;
    const char* v = std::getenv("NJCX_TRACE_SHARED");
    if (!v) {
        enabled = 0;
        return false;
    }
    enabled = (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 || std::strcmp(v, "TRUE") == 0) ? 1 : 0;
    return enabled != 0;
}

uint64_t hashVec2Array(const ::nicxlive::core::nodes::Vec2Array& arr) {
    constexpr uint64_t kOffset = 1469598103934665603ull;
    constexpr uint64_t kPrime = 1099511628211ull;
    uint64_t h = kOffset;
    const float* xs = arr.dataX();
    const float* ys = arr.dataY();
    if (!xs || !ys) return h;
    for (std::size_t i = 0; i < arr.size(); ++i) {
        uint32_t bx = 0, by = 0;
        std::memcpy(&bx, &xs[i], sizeof(float));
        std::memcpy(&by, &ys[i], sizeof(float));
        h ^= static_cast<uint64_t>(bx);
        h *= kPrime;
        h ^= static_cast<uint64_t>(by);
        h *= kPrime;
    }
    return h;
}
} // namespace

// --- QueueCommandEmitter (繧ｭ繝･繝ｼ蜿朱寔逕ｨ) ---

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
    if (traceSharedEnabled()) {
        using ::nicxlive::core::render::sharedDeformBufferData;
        auto& deform = sharedDeformBufferData();
        const uint64_t h = hashVec2Array(deform);
        std::fprintf(stderr,
                     "[nicxlive][shared-hash] queue=%zu deformStride=%zu hash=%llu\n",
                     backend_ ? backend_->queue.size() : 0,
                     deform.size(),
                     static_cast<unsigned long long>(h));
    }
    activeBackend_ = nullptr;
    pendingMask = false;
    pendingMaskUsesStencil = false;
}

void QueueCommandEmitter::beginMask(bool useStencil) {
    // D 迚医〒縺ｯ BeginMask 縺ｯ ApplyMask 縺梧・遶九＠縺溘→縺阪↓蛻昴ａ縺ｦ繧ｭ繝･繝ｼ縺ｸ遨阪・
    pendingMask = true;
    pendingMaskUsesStencil = useStencil;
}

void QueueCommandEmitter::applyMask(const std::shared_ptr<nodes::Drawable>& mask, bool dodge) {
    if (!mask) return;
    RenderBackend::MaskApplyPacket packet{};
    if (!tryMakeMaskApplyPacket(mask, dodge, packet)) {
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
        // D迚・ BeginMask 縺ｯ ApplyMask 縺梧・遶九＠縺溘→縺阪・縺ｿ繧ｭ繝･繝ｼ縺ｸ遨阪・縲・
        // ApplyMask 縺梧擂縺壹↓ BeginMaskContent 縺縺第擂縺溷ｴ蜷医・菴輔ｂ縺帙★遐ｴ譽・・
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
    if (packet.textureUUIDs[0] == 0 && packet.textureUUIDs[1] == 0 && packet.textureUUIDs[2] == 0) {
        if (auto node = packet.node.lock()) {
            std::fprintf(stderr, "[nicxlive] enqueue tex0 uuid=%u vo=%u idx=%u vtx=%u isMask=%d\n",
                         node->uuid, packet.vertexOffset, packet.indexCount, packet.vertexCount, packet.isMask ? 1 : 0);
        } else {
            std::fprintf(stderr, "[nicxlive] enqueue tex0 uuid=<expired> vo=%u idx=%u vtx=%u isMask=%d\n",
                         packet.vertexOffset, packet.indexCount, packet.vertexCount, packet.isMask ? 1 : 0);
        }
    }
    QueuedCommand cmd{};
    cmd.kind = RenderCommandKind::DrawPart;
    cmd.partPacket = packet;
    if (backend_) {
        if (packet.vertexOffset == 1621 || packet.vertexOffset == 1563 || packet.vertexOffset == 1504) {
            std::fprintf(stderr, "[nicxlive] emit push idx=%zu kind=DrawPart vo=%u\n",
                         backend_->queue.size(),
                         packet.vertexOffset);
        }
        backend_->queue.push_back(std::move(cmd));
    }
}

void QueueCommandEmitter::beginDynamicComposite(const std::shared_ptr<nodes::Projectable>&, const DynamicCompositePass& pass) {
    QueuedCommand cmd{};
    cmd.kind = RenderCommandKind::BeginDynamicComposite;
    cmd.dynamicPass = pass;
    if (backend_) {
        std::fprintf(stderr, "[nicxlive] emit push idx=%zu kind=BeginDynamicComposite stencil=%u\n",
                     backend_->queue.size(),
                     pass.stencil ? pass.stencil->backendId() : 0u);
        backend_->queue.push_back(std::move(cmd));
    }
}

void QueueCommandEmitter::endDynamicComposite(const std::shared_ptr<nodes::Projectable>&, const DynamicCompositePass& pass) {
    QueuedCommand cmd{};
    cmd.kind = RenderCommandKind::EndDynamicComposite;
    cmd.dynamicPass = pass;
    if (backend_) {
        std::fprintf(stderr, "[nicxlive] emit push idx=%zu kind=EndDynamicComposite stencil=%u\n",
                     backend_->queue.size(),
                     pass.stencil ? pass.stencil->backendId() : 0u);
        backend_->queue.push_back(std::move(cmd));
    }
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


