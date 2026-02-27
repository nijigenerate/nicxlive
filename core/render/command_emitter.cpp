#include "command_emitter.hpp"
#include "backend_queue.hpp"
#include "../runtime_state.hpp"
#include "../debug_log.hpp"
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

bool traceDrawMapEnabled() {
    static int enabled = -1;
    if (enabled >= 0) return enabled != 0;
    const char* v = std::getenv("NJCX_TRACE_DRAWPART_MAP");
    if (!v) {
        enabled = 0;
        return false;
    }
    enabled = (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 || std::strcmp(v, "TRUE") == 0) ? 1 : 0;
    return enabled != 0;
}

int traceDrawMapMaxFrames() {
    static int parsed = 0;
    static int value = 1;
    if (parsed) return value;
    parsed = 1;
    const char* v = std::getenv("NJCX_TRACE_DRAWPART_MAP_FRAMES");
    if (!v) return value;
    int out = std::atoi(v);
    if (out > 0) value = out;
    return value;
}

uint64_t hashVec2Array(const ::nicxlive::core::nodes::Vec2Array& arr) {
    constexpr uint64_t kOffset = 1469598103934665603ull;
    constexpr uint64_t kPrime = 1099511628211ull;
    uint64_t h = kOffset;
    for (std::size_t i = 0; i < arr.size(); ++i) {
        uint32_t bx = 0, by = 0;
        const float x = arr.xAt(i);
        const float y = arr.yAt(i);
        std::memcpy(&bx, &x, sizeof(float));
        std::memcpy(&by, &y, sizeof(float));
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
    static int frameIndex = 0;
    activeBackend_ = backend;
    statePtr_ = &state;
    state = RenderGpuState::init();
    dynDepth_ = 0;
    dynStack_.clear();
    cameraStack_.clear();
    currentCamera_ = ::nicxlive::core::inGetCamera();
    pendingMask = false;
    pendingMaskUsesStencil = false;
    if (backend_) {
        backend_->queue.clear();
    }
    if (traceDrawMapEnabled()) {
        NJCX_DBG_LOG("[nicxlive][drawmap] begin frame=%d\n", frameIndex);
    }
    frameIndex++;
    uploadSharedBuffers();
}

void QueueCommandEmitter::endFrame(RenderBackend*, RenderGpuState&) {
    if (traceSharedEnabled()) {
        using ::nicxlive::core::render::sharedDeformBufferData;
        auto& deform = sharedDeformBufferData();
        const uint64_t h = hashVec2Array(deform);
        NJCX_DBG_LOG("[nicxlive][shared-hash] queue=%zu deformStride=%zu hash=%llu\n",
                     backend_ ? backend_->queue.size() : 0,
                     deform.size(),
                     static_cast<unsigned long long>(h));
    }
    activeBackend_ = nullptr;
    statePtr_ = nullptr;
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
    if (traceDrawMapEnabled() && backend_) {
        static int frameCounter = 0;
        if (backend_->queue.empty()) {
            frameCounter++;
        }
        if (frameCounter <= traceDrawMapMaxFrames()) {
            const auto idx = backend_->queue.size();
            auto node = packet.node.lock();
            const char* nodeName = node ? node->name.c_str() : "<expired>";
            const unsigned nodeUuid = node ? node->uuid : 0u;
            float d0x = 0.0f;
            float d0y = 0.0f;
            if (packet.deformation.size() > 0) {
                d0x = packet.deformation.xAt(0);
                d0y = packet.deformation.yAt(0);
            }
            NJCX_DBG_LOG(
                "[nicxlive][drawmap] cmd=%zu node=%s(%u) tex0=%u vo=%u uo=%u do=%u vtx=%u idx=%u d0=(%g,%g)\n",
                idx,
                nodeName,
                nodeUuid,
                packet.textureUUIDs[0],
                packet.vertexOffset,
                packet.uvOffset,
                packet.deformOffset,
                packet.vertexCount,
                packet.indexCount,
                d0x,
                d0y
            );
        }
    }
    QueuedCommand cmd{};
    cmd.kind = RenderCommandKind::DrawPart;
    cmd.partPacket = packet;
    if (backend_) {
        backend_->queue.push_back(std::move(cmd));
    }
}

void QueueCommandEmitter::beginDynamicComposite(const std::shared_ptr<nodes::Projectable>&, const DynamicCompositePass& pass) {
    dynDepth_++;
    auto passData = pass;
    int vw = 0;
    int vh = 0;
    ::nicxlive::core::inGetViewport(vw, vh);
    passData.origViewport[0] = 0;
    passData.origViewport[1] = 0;
    passData.origViewport[2] = vw;
    passData.origViewport[3] = vh;
    passData.origBuffer = statePtr_ ? statePtr_->framebuffer : 0;
    dynStack_.push_back(passData);

    cameraStack_.push_back(::nicxlive::core::inGetCamera());
    if (passData.surface && passData.surface->textureCount > 0) {
        auto tex = passData.surface->textures[0];
        if (tex) {
            ::nicxlive::core::inPushViewport(tex->width(), tex->height());
            auto camera = ::nicxlive::core::inGetCamera();
            camera.scale = nodes::Vec2{1.0f, -1.0f};
            const float invScaleX = passData.scale.x == 0.0f ? 0.0f : 1.0f / passData.scale.x;
            const float invScaleY = passData.scale.y == 0.0f ? 0.0f : 1.0f / passData.scale.y;
            const auto scaling = nodes::Mat4::scale(nodes::Vec3{invScaleX, invScaleY, 1.0f});
            const auto rotation = nodes::Mat4::zRotation(-passData.rotationZ);
            const auto offsetMatrix = nodes::Mat4::multiply(scaling, rotation);
            const auto offsetOrigin = offsetMatrix.transformPoint(nodes::Vec3{0.0f, 0.0f, 0.0f});
            camera.position = nodes::Vec2{-offsetOrigin.x, -offsetOrigin.y};
            ::nicxlive::core::inSetCamera(camera);
        }
    }

    QueuedCommand cmd{};
    cmd.kind = RenderCommandKind::BeginDynamicComposite;
    cmd.dynamicPass = passData;
    if (backend_) {
        NJCX_DBG_LOG("[nicxlive] emit push idx=%zu kind=BeginDynamicComposite stencil=%u\n",
                     backend_->queue.size(),
                     passData.stencil ? passData.stencil->backendId() : 0u);
        backend_->queue.push_back(std::move(cmd));
    }
}

void QueueCommandEmitter::endDynamicComposite(const std::shared_ptr<nodes::Projectable>&, const DynamicCompositePass& pass) {
    auto passData = pass;
    if (dynDepth_ > 0) {
        dynDepth_--;
    }
    if (!dynStack_.empty()) {
        passData = dynStack_.back();
        dynStack_.pop_back();
    }
    if (passData.surface && passData.surface->textureCount > 0 && passData.surface->textures[0]) {
        ::nicxlive::core::inPopViewport();
    }
    if (!cameraStack_.empty()) {
        ::nicxlive::core::inSetCamera(cameraStack_.back());
        cameraStack_.pop_back();
    }

    QueuedCommand cmd{};
    cmd.kind = RenderCommandKind::EndDynamicComposite;
    cmd.dynamicPass = passData;
    if (backend_) {
        NJCX_DBG_LOG("[nicxlive] emit push idx=%zu kind=EndDynamicComposite stencil=%u\n",
                     backend_->queue.size(),
                     passData.stencil ? passData.stencil->backendId() : 0u);
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
        auto& vertices = sharedVertexBufferData();
        if (vertices.size()) {
            activeBackend_->uploadSharedVertexBuffer(vertices);
        }
        sharedVertexMarkUploaded();
    }
    if (sharedUvBufferDirty()) {
        auto& uvs = sharedUvBufferData();
        if (uvs.size()) {
            activeBackend_->uploadSharedUvBuffer(uvs);
        }
        sharedUvMarkUploaded();
    }
    if (sharedDeformBufferDirty()) {
        auto& deform = sharedDeformBufferData();
        if (deform.size()) {
            activeBackend_->uploadSharedDeformBuffer(deform);
        }
        sharedDeformMarkUploaded();
    }
}

} // namespace nicxlive::core::render
