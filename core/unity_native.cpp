// unity_native: C API bridging for Unity plugin (queue backend path)

#include "unity_native.hpp"

#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <string>
#if defined(_WIN32)
#include <malloc.h>
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "Psapi.lib")
#include <cstdio>
#elif defined(__APPLE__)
#include <malloc/malloc.h>
#endif
#include <cstring>
#include "runtime_state.hpp"
#include "timing.hpp"
#include "render/profiler.hpp"
#include "debug_log.hpp"
#include "render/shared_deform_buffer.hpp"

using namespace nicxlive::core;
using namespace nicxlive::core::render;
namespace nodes = ::nicxlive::core::nodes;

namespace {
struct RendererCtx {
    UnityRendererConfig cfg{};
    UnityResourceCallbacks callbacks{};
    std::shared_ptr<QueueRenderBackend> backend{std::make_shared<QueueRenderBackend>()};
    std::unique_ptr<RenderCommandEmitter> emitter{};
    std::unique_ptr<RenderGraphBuilder> graph{};
    std::unique_ptr<TaskScheduler> scheduler{};
    RenderContext ctx{};
    std::vector<QueuedCommand> recorded{}; // last emitted commands
    TextureStats stats{0, 0, 0};
    SharedBufferSnapshot shared{};
    std::vector<float> packedVertices{};
    std::vector<float> packedUvs{};
    std::vector<float> packedDeform{};
    std::vector<NjgQueuedCommand> queued{};
    std::vector<void*> puppetHandles{};
    // Runtime texture UUIDs (loaded from puppet slots) -> Unity texture handles
    std::unordered_map<uint32_t, size_t> runtimeTextureHandles{};
    // Queue backend texture IDs (including dynamic RT/stencil) -> Unity texture handles
    std::unordered_map<uint32_t, size_t> backendTextureHandles{};
    uint32_t syntheticTextureIdCounter{0x70000000u};
    size_t renderHandle{0};
    size_t compositeHandle{0};
    int lastViewportW{0};
    int lastViewportH{0};
    struct AnimationState {
        bool playing{false};
        bool paused{false};
        bool loop{false};
        bool stopping{false};
        bool playLeadOut{false};
        int frame{0};
        int looped{0};
    };
    std::unordered_map<void*, std::unordered_map<std::string, AnimationState>> animationStates{};
};

struct PuppetCtx {
    std::shared_ptr<Puppet> puppet{};
};

std::mutex gMutex;
std::unordered_map<void*, std::unique_ptr<RendererCtx>> gRenderers;
std::unordered_map<void*, std::unique_ptr<PuppetCtx>> gPuppets;
NjgLogFn gLogCallback{nullptr};
void* gLogUserData{nullptr};
bool gRuntimeInitialized{false};
double gUnityTimeTicker{0.0};

bool perfEnabled() {
    static int enabled = -1;
    if (enabled >= 0) return enabled != 0;
    const char* v = std::getenv("NJCX_PROFILE");
    if (!v) {
        enabled = 0;
        return false;
    }
    enabled = (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 || std::strcmp(v, "TRUE") == 0) ? 1 : 0;
    return enabled != 0;
}

struct UnityPerfWindow {
    std::chrono::steady_clock::time_point lastReport{};
    std::size_t tickFrames{0};
    double tickMs{0.0};
    double emitMs{0.0};
    double sharedMs{0.0};
    double packMs{0.0};
    double textureCmdMs{0.0};

    void addTick(double ms) {
        if (!perfEnabled()) return;
        tickFrames++;
        tickMs += ms;
        maybeReport();
    }

    void addEmit(double emit, double pack, double textureCmd) {
        if (!perfEnabled()) return;
        emitMs += emit;
        packMs += pack;
        textureCmdMs += textureCmd;
        maybeReport();
    }

    void addShared(double ms) {
        if (!perfEnabled()) return;
        sharedMs += ms;
        maybeReport();
    }

    void maybeReport() {
        auto now = std::chrono::steady_clock::now();
        if (lastReport.time_since_epoch().count() == 0) {
            lastReport = now;
            return;
        }
        if ((now - lastReport) < std::chrono::seconds(1)) return;
        const double frameDiv = tickFrames ? static_cast<double>(tickFrames) : 1.0;
        std::fprintf(stderr,
                     "[nicxlive][perf/unity] frames=%zu tick=%.3fms emit=%.3fms shared=%.3fms pack=%.3fms texcmd=%.3fms\n",
                     tickFrames,
                     tickMs / frameDiv,
                     emitMs / frameDiv,
                     sharedMs / frameDiv,
                     packMs / frameDiv,
                     textureCmdMs / frameDiv);
        tickFrames = 0;
        tickMs = 0.0;
        emitMs = 0.0;
        sharedMs = 0.0;
        packMs = 0.0;
        textureCmdMs = 0.0;
        lastReport = now;
    }
};

UnityPerfWindow& unityPerfWindow() {
    static UnityPerfWindow window;
    return window;
}

void unityLog(const std::string& message) {
    if (gLogCallback) {
        gLogCallback(message.c_str(), message.size(), gLogUserData);
    }
}

template <typename T>
void destroyHandle(std::unordered_map<void*, std::unique_ptr<T>>& map, void* h) {
    std::lock_guard<std::mutex> lock(gMutex);
    map.erase(h);
}

RendererCtx::AnimationState* findAnimationState(RendererCtx& renderer, void* puppet, const std::string& name) {
    auto pit = renderer.animationStates.find(puppet);
    if (pit == renderer.animationStates.end()) return nullptr;
    auto ait = pit->second.find(name);
    if (ait == pit->second.end()) return nullptr;
    return &ait->second;
}

RendererCtx::AnimationState* ensureAnimationState(RendererCtx& renderer, void* puppet, const std::string& name) {
    return &renderer.animationStates[puppet][name];
}

static void applyTextureCommands(RendererCtx& ctx) {
    if (!ctx.callbacks.createTexture && !ctx.callbacks.updateTexture && !ctx.callbacks.releaseTexture) return;
    for (const auto& rc : ctx.backend->resourceQueue) {
        switch (rc.kind) {
        case TextureCommandKind::Create: {
            break;
        }
        case TextureCommandKind::Update: {
            break;
        }
        case TextureCommandKind::Params:
            // Unity-side texture parameter callback API is not exposed here.
            break;
        case TextureCommandKind::Dispose: {
            auto it = ctx.backendTextureHandles.find(rc.id);
            if (it != ctx.backendTextureHandles.end()) {
                if (ctx.callbacks.releaseTexture) {
                    ctx.callbacks.releaseTexture(it->second, ctx.callbacks.userData);
                }
                ctx.backendTextureHandles.erase(it);
                ctx.stats.released++;
                if (ctx.stats.current > 0) ctx.stats.current--;
            }
            break;
        }
        }
    }
    ctx.backend->resourceQueue.clear();
}

static uint32_t allocateSyntheticTextureId(RendererCtx& ctx) {
    uint32_t id = ctx.syntheticTextureIdCounter;
    for (uint32_t guard = 0; guard < 0x100000; ++guard) {
        ++id;
        if (id == 0) continue;
        if (ctx.runtimeTextureHandles.find(id) == ctx.runtimeTextureHandles.end()) {
            ctx.syntheticTextureIdCounter = id;
            return id;
        }
    }
    return 0;
}

static void ensurePuppetTextures(RendererCtx& ctx, const std::shared_ptr<Puppet>& puppet) {
    if (!puppet || !ctx.callbacks.createTexture) return;
    for (const auto& tex : puppet->textureSlots) {
        if (!tex) continue;
        uint32_t uuid = tex->getRuntimeUUID();
        if (uuid == 0) {
            uuid = allocateSyntheticTextureId(ctx);
            if (uuid != 0) tex->setRuntimeUUID(uuid);
        }
        if (uuid == 0) continue;
        if (ctx.runtimeTextureHandles.find(uuid) != ctx.runtimeTextureHandles.end()) continue;
        size_t handle = ctx.callbacks.createTexture(tex->width(), tex->height(), tex->channels(), 1, tex->channels(),
                                                    /*renderTarget*/ false, tex->stencil(), ctx.callbacks.userData);
        if (handle != 0) {
            ctx.runtimeTextureHandles[uuid] = handle;
            const auto& data = tex->data();
            if (!data.empty() && ctx.callbacks.updateTexture) {
                ctx.callbacks.updateTexture(handle, data.data(), data.size(), tex->width(), tex->height(), tex->channels(), ctx.callbacks.userData);
            }
            ctx.stats.created++;
            ctx.stats.current++;
        }
    }
}

static size_t ensureDynamicTextureHandle(RendererCtx& ctx, const std::shared_ptr<Texture>& tex, bool renderTarget, bool stencil) {
    if (!tex) return 0;
    const uint32_t backendId = tex->backendId();
    if (backendId == 0) return 0;

    auto found = ctx.backendTextureHandles.find(backendId);
    if (found != ctx.backendTextureHandles.end()) {
        return found->second;
    }

    if (!ctx.callbacks.createTexture || !ctx.callbacks.updateTexture) {
        return backendId;
    }

    const int w = tex->width();
    const int h = tex->height();
    const int c = tex->channels();
    size_t handle = ctx.callbacks.createTexture(w, h, c, 1, c, renderTarget, stencil, ctx.callbacks.userData);
    if (handle == 0) {
        return backendId;
    }

    ctx.backendTextureHandles[backendId] = handle;
    ctx.stats.created++;
    ctx.stats.current++;

    std::vector<uint8_t> upload;
    std::size_t expected = 0;
    if (w > 0 && h > 0 && c > 0) {
        expected = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * static_cast<std::size_t>(c);
    }
    if (expected > 0) {
        upload.resize(expected, 0);
        const auto& data = tex->data();
        if (!data.empty()) {
            auto copyLen = (std::min)(expected, data.size());
            std::copy_n(data.begin(), copyLen, upload.begin());
        }
        ctx.callbacks.updateTexture(handle, upload.data(), upload.size(), w, h, c, ctx.callbacks.userData);
    } else {
        ctx.callbacks.updateTexture(handle, nullptr, 0, w, h, c, ctx.callbacks.userData);
    }

    return handle;
}


static void releaseExternalTexture(RendererCtx& ctx, size_t handle) {
    if (handle == 0) return;
    if (ctx.callbacks.releaseTexture) {
        ctx.callbacks.releaseTexture(handle, ctx.callbacks.userData);
    }
    ctx.stats.released++;
    if (ctx.stats.current > 0) ctx.stats.current--;
}

static void releasePuppetTextures(RendererCtx& ctx, const std::shared_ptr<Puppet>& puppet) {
    if (!puppet) return;
    for (const auto& tex : puppet->textureSlots) {
        if (!tex) continue;
        uint32_t uuid = tex->getRuntimeUUID();
        if (uuid == 0) continue;
        auto it = ctx.runtimeTextureHandles.find(uuid);
        if (it != ctx.runtimeTextureHandles.end()) {
            releaseExternalTexture(ctx, it->second);
            ctx.runtimeTextureHandles.erase(it);
        }
    }
}

} // namespace

// Helpers to pack SoA buffers into contiguous floats (x lane then y lane)
static void packVec2Array(const ::nicxlive::core::nodes::Vec2Array& src, std::vector<float>& dst) {
    auto n = src.size();
    dst.resize(n * 2);
    const float* xs = src.xPtr_;
    const float* ys = src.yPtr_;
    if (!xs || !ys) return;
    for (std::size_t i = 0; i < n; ++i) {
        dst[i] = xs[i];
        dst[n + i] = ys[i];
    }
}

static const std::vector<uint16_t>* resolvePacketIndices(const RendererCtx& ctx, const nodes::PartDrawPacket& packet) {
    if (auto idxBuf = ctx.backend->getDrawableIndices(packet.indexBuffer); idxBuf && !idxBuf->empty()) {
        return idxBuf;
    }
    auto node = packet.node.lock();
    if (!node || node->mesh->indices.empty()) {
        return nullptr;
    }
    return &node->mesh->indices;
}

static size_t resolvePacketTextureHandle(const RendererCtx& ctx, const nodes::PartDrawPacket& packet, std::size_t textureIndex) {
    if (textureIndex >= 3) {
        return 0;
    }

    const auto packetRuntimeId = packet.textureUUIDs[textureIndex];
    if (packetRuntimeId != 0) {
        auto runtimeHit = ctx.runtimeTextureHandles.find(packetRuntimeId);
        if (runtimeHit != ctx.runtimeTextureHandles.end()) {
            return runtimeHit->second;
        }
        auto backendHit = ctx.backendTextureHandles.find(packetRuntimeId);
        if (backendHit != ctx.backendTextureHandles.end()) {
            return backendHit->second;
        }
    }

    const auto backendId = packet.textureBackendIds[textureIndex];
    if (backendId != 0) {
        auto backendHit = ctx.backendTextureHandles.find(backendId);
        if (backendHit != ctx.backendTextureHandles.end()) {
            return backendHit->second;
        }
    }

    auto node = packet.node.lock();
    if (!node || textureIndex >= node->textures.size()) {
        return 0;
    }
    const auto& texture = node->textures[textureIndex];
    if (!texture) {
        return 0;
    }

    const auto runtimeId = texture->getRuntimeUUID();
    if (runtimeId != 0) {
        auto runtimeHit = ctx.runtimeTextureHandles.find(runtimeId);
        if (runtimeHit != ctx.runtimeTextureHandles.end()) {
            return runtimeHit->second;
        }
    }

    const auto textureBackendId = texture->backendId();
    if (textureBackendId != 0) {
        auto backendHit = ctx.backendTextureHandles.find(textureBackendId);
        if (backendHit != ctx.backendTextureHandles.end()) {
            return backendHit->second;
        }
    }

    return 0;
}

static void packPartPacket(RendererCtx& ctx, const nodes::PartDrawPacket& src, NjgPartDrawPacket& dst, std::size_t textureCount) {
    dst.isMask = src.isMask;
    dst.renderable = src.renderable;
    dst.modelMatrix = src.modelMatrix;
    dst.renderMatrix = src.renderMatrix;
    dst.renderRotation = src.renderRotation;
    dst.clampedTint = src.clampedTint;
    dst.clampedScreen = src.clampedScreen;
    dst.opacity = src.opacity;
    dst.emissionStrength = src.emissionStrength;
    dst.maskThreshold = src.maskThreshold;
    dst.blendingMode = static_cast<int>(src.blendMode);
    dst.useMultistageBlend = src.useMultistageBlend;
    dst.hasEmissionOrBumpmap = src.hasEmissionOrBumpmap;
    dst.origin = src.origin;
    dst.vertexOffset = src.vertexOffset;
    dst.vertexAtlasStride = src.vertexAtlasStride;
    dst.uvOffset = src.uvOffset;
    dst.uvAtlasStride = src.uvAtlasStride;
    dst.deformOffset = src.deformOffset;
    dst.deformAtlasStride = src.deformAtlasStride;
    dst.indexHandle = src.indexBuffer;

    if (const auto* indices = resolvePacketIndices(ctx, src); indices && !indices->empty()) {
        dst.indexCount = (std::min<std::size_t>)(src.indexCount, indices->size());
        dst.indices = indices->data();
    } else {
        dst.indexCount = 0;
        dst.indices = nullptr;
    }
    dst.vertexCount = src.vertexCount;

    if (textureCount > 3) {
        textureCount = 3;
    }
    for (std::size_t i = 0; i < textureCount; ++i) {
        dst.textureHandles[i] = resolvePacketTextureHandle(ctx, src, i);
    }
    for (std::size_t i = textureCount; i < 3; ++i) {
        dst.textureHandles[i] = 0;
    }
    dst.textureCount = textureCount;
}

static void packQueuedCommands(RendererCtx& ctx) {
    auto profile = render::profileScope("Unity.packQueuedCommands");
    ctx.queued.clear();
    for (const auto& qc : ctx.backend->queue) {
        NjgQueuedCommand out{};
        switch (qc.kind) {
        case RenderCommandKind::DrawPart: out.kind = NjgRenderCommandKind::DrawPart; break;
        case RenderCommandKind::BeginDynamicComposite: out.kind = NjgRenderCommandKind::BeginDynamicComposite; break;
        case RenderCommandKind::EndDynamicComposite: out.kind = NjgRenderCommandKind::EndDynamicComposite; break;
        case RenderCommandKind::BeginMask: out.kind = NjgRenderCommandKind::BeginMask; break;
        case RenderCommandKind::ApplyMask: out.kind = NjgRenderCommandKind::ApplyMask; break;
        case RenderCommandKind::BeginMaskContent: out.kind = NjgRenderCommandKind::BeginMaskContent; break;
        case RenderCommandKind::EndMask: out.kind = NjgRenderCommandKind::EndMask; break;
        default: out.kind = NjgRenderCommandKind::DrawPart; break;
        }
        // Part packet
        const auto& pp = qc.partPacket;
        packPartPacket(ctx, pp, out.partPacket, (qc.kind == RenderCommandKind::DrawPart) ? 3 : 0);
        // Mask apply
        switch (qc.maskApplyPacket.kind) {
        case nicxlive::core::RenderBackend::MaskDrawableKind::Part:
            out.maskApplyPacket.kind = ::MaskDrawableKind::Part;
            break;
        case nicxlive::core::RenderBackend::MaskDrawableKind::Mask:
            out.maskApplyPacket.kind = ::MaskDrawableKind::Mask;
            break;
        case nicxlive::core::RenderBackend::MaskDrawableKind::Drawable:
        default:
            out.maskApplyPacket.kind = ::MaskDrawableKind::Part;
            break;
        }
        out.maskApplyPacket.isDodge = qc.maskApplyPacket.isDodge;
        // D parity: ApplyMask serializes its own part packet, not the current DrawPart packet.
        const auto& applyPart = qc.maskApplyPacket.partPacket;
        auto& maskPart = out.maskApplyPacket.partPacket;
        packPartPacket(ctx, applyPart, maskPart, 3);
        // mask packet from qc.maskApplyPacket.maskPacket
        const auto& mp = qc.maskApplyPacket.maskPacket;
        out.maskApplyPacket.maskPacket.modelMatrix = mp.modelMatrix;
        out.maskApplyPacket.maskPacket.mvp = mp.mvp;
        out.maskApplyPacket.maskPacket.origin = mp.origin;
        out.maskApplyPacket.maskPacket.vertexOffset = mp.vertexOffset;
        out.maskApplyPacket.maskPacket.vertexAtlasStride = mp.vertexAtlasStride;
        out.maskApplyPacket.maskPacket.deformOffset = mp.deformOffset;
        out.maskApplyPacket.maskPacket.deformAtlasStride = mp.deformAtlasStride;
        out.maskApplyPacket.maskPacket.indexHandle = mp.indexBuffer;
        auto midx = ctx.backend->getDrawableIndices(mp.indexBuffer);
        if (midx && !midx->empty()) {
            out.maskApplyPacket.maskPacket.indexCount = std::min<std::size_t>(mp.indexCount, midx->size());
            out.maskApplyPacket.maskPacket.indices = midx->data();
            out.maskApplyPacket.maskPacket.vertexCount = mp.vertexCount;
        } else {
            out.maskApplyPacket.maskPacket.indexCount = 0;
            out.maskApplyPacket.maskPacket.vertexCount = 0;
            out.maskApplyPacket.maskPacket.indices = nullptr;
        }

        // Dynamic pass (D parity)
        size_t dynTexCount = qc.dynamicPass.surface
            ? qc.dynamicPass.surface->textureCount
            : qc.dynamicPass.textures.size();
        if (dynTexCount > 3) dynTexCount = 3;
        out.dynamicPass.textureCount = dynTexCount;
        for (size_t i = 0; i < dynTexCount; ++i) {
            std::shared_ptr<Texture> tex{};
            if (qc.dynamicPass.surface) {
                tex = qc.dynamicPass.surface->textures[i];
            } else if (i < qc.dynamicPass.textures.size()) {
                tex = qc.dynamicPass.textures[i];
            }
            out.dynamicPass.textures[i] = ensureDynamicTextureHandle(ctx, tex, /*renderTarget*/ true, /*stencil*/ false);
        }
        for (size_t i = dynTexCount; i < 3; ++i) {
            out.dynamicPass.textures[i] = 0;
        }
        auto stencilTex = qc.dynamicPass.stencil;
        if (!stencilTex && qc.dynamicPass.surface) {
            stencilTex = qc.dynamicPass.surface->stencil;
        }
        out.dynamicPass.stencil = ensureDynamicTextureHandle(ctx, stencilTex, /*renderTarget*/ true, /*stencil*/ true);
        out.dynamicPass.scale = qc.dynamicPass.scale;
        out.dynamicPass.rotationZ = qc.dynamicPass.rotationZ;
        out.dynamicPass.autoScaled = qc.dynamicPass.autoScaled;
        out.dynamicPass.origBuffer = qc.dynamicPass.origBuffer;
        for (int i = 0; i < 4; ++i) out.dynamicPass.origViewport[i] = qc.dynamicPass.origViewport[i];
        if (out.dynamicPass.origViewport[2] == 0 || out.dynamicPass.origViewport[3] == 0) {
            int vw = 0;
            int vh = 0;
            inGetViewport(vw, vh);
            out.dynamicPass.origViewport[0] = 0;
            out.dynamicPass.origViewport[1] = 0;
            out.dynamicPass.origViewport[2] = vw;
            out.dynamicPass.origViewport[3] = vh;
        }
        out.dynamicPass.drawBufferCount = qc.dynamicPass.surface
            ? static_cast<int>(std::max<std::size_t>(qc.dynamicPass.surface->textureCount, 1))
            : 0;
        out.dynamicPass.hasStencil = (qc.dynamicPass.surface && qc.dynamicPass.surface->stencil) || (out.dynamicPass.stencil != 0);
        out.usesStencil = qc.usesStencil;
        ctx.queued.push_back(out);
    }
}

extern "C" {

void njgRuntimeInit() {
    std::lock_guard<std::mutex> lock(gMutex);
    if (gRuntimeInitialized) return;
    gUnityTimeTicker = 0.0;
    gRuntimeInitialized = true;
    inSetTimingFunc([]() {
        return gUnityTimeTicker;
    });
}

void njgRuntimeTerm() {
    std::lock_guard<std::mutex> lock(gMutex);
    gRenderers.clear();
    gPuppets.clear();
    gRuntimeInitialized = false;
}

NjgResult njgCreateRenderer(const UnityRendererConfig* config, const UnityResourceCallbacks* callbacks, void** outRenderer) {
    if (!config || !outRenderer) return NjgResult::InvalidArgument;
    njgRuntimeInit();
    auto ctx = std::make_unique<RendererCtx>();
    ctx->cfg = *config;
    if (callbacks) ctx->callbacks = *callbacks;
    ctx->graph = std::make_unique<RenderGraphBuilder>();
    ctx->scheduler = std::make_unique<TaskScheduler>();
    ctx->emitter = std::make_unique<RenderCommandEmitter>();
    ctx->ctx.renderGraph = ctx->graph.get();
    ctx->ctx.renderBackend = ctx->backend.get();
    ctx->ctx.gpuState = RenderGpuState::init();
    setCurrentRenderBackend(ctx->backend);
    initRendererCommon();
    ctx->backend->initializeRenderer();
    if (config->viewportWidth > 0 && config->viewportHeight > 0) {
        inSetViewport(config->viewportWidth, config->viewportHeight);
    }
    void* handle = ctx.get();
    {
        std::lock_guard<std::mutex> lock(gMutex);
        gRenderers[handle] = std::move(ctx);
    }
    *outRenderer = handle;
    return NjgResult::Ok;
}

void njgDestroyRenderer(void* renderer) {
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gRenderers.find(renderer);
    if (it != gRenderers.end()) {
        // release RTs
        releaseExternalTexture(*it->second, it->second->renderHandle);
        releaseExternalTexture(*it->second, it->second->compositeHandle);
        // release remaining external textures (dedup handles shared across maps)
        std::unordered_set<size_t> released{};
        for (const auto& kv : it->second->runtimeTextureHandles) {
            if (kv.second != 0 && released.insert(kv.second).second) {
                releaseExternalTexture(*it->second, kv.second);
            }
        }
        for (const auto& kv : it->second->backendTextureHandles) {
            if (kv.second != 0 && released.insert(kv.second).second) {
                releaseExternalTexture(*it->second, kv.second);
            }
        }
        it->second->runtimeTextureHandles.clear();
        it->second->backendTextureHandles.clear();
        it->second->animationStates.clear();
        gRenderers.erase(it);
    }
}

NjgResult njgLoadPuppet(void* renderer, const char* pathUtf8, void** outPuppet) {
    if (!renderer || !pathUtf8 || !outPuppet) return NjgResult::InvalidArgument;
    try {
        auto pup = fmt::inLoadPuppet<Puppet>(pathUtf8);
        if (auto root = pup->actualRoot()) {
            std::function<void(const std::shared_ptr<nodes::Node>&)> markProjectablesIgnorePuppet;
            markProjectablesIgnorePuppet = [&](const std::shared_ptr<nodes::Node>& n) {
                if (!n) return;
                if (auto proj = std::dynamic_pointer_cast<nodes::Projectable>(n)) {
                    proj->setIgnorePuppet(true);
                }
                for (const auto& child : n->childrenList()) {
                    markProjectablesIgnorePuppet(child);
                }
            };
            markProjectablesIgnorePuppet(root);
            pup->rescanNodes();
            root->build(true);
        }
        auto ctx = std::make_unique<PuppetCtx>();
        ctx->puppet = pup;
        void* handle = ctx.get();
        {
            std::lock_guard<std::mutex> lock(gMutex);
            gPuppets[handle] = std::move(ctx);
            auto rit = gRenderers.find(renderer);
            if (rit != gRenderers.end()) {
                pup->setRenderBackend(rit->second->backend);
                rit->second->puppetHandles.push_back(handle);
                ensurePuppetTextures(*rit->second, pup);
            }
        }
        *outPuppet = handle;
        return NjgResult::Ok;
    } catch (const std::exception& ex) {
        unityLog(std::string("[nicxlive] njgLoadPuppet exception: ") + ex.what());
        return NjgResult::Failure;
    } catch (...) {
        unityLog("[nicxlive] njgLoadPuppet exception: unknown");
        return NjgResult::Failure;
    }
}

NjgResult njgGetParameters(void* puppetHandle, NjgParameterInfo* buffer, size_t bufferLength, size_t* outCount) {
    if (!outCount) return NjgResult::InvalidArgument;
    *outCount = 0;
    std::shared_ptr<Puppet> pup;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        auto it = gPuppets.find(puppetHandle);
        if (it == gPuppets.end() || !it->second || !it->second->puppet) return NjgResult::InvalidArgument;
        pup = it->second->puppet;
    }
    const auto& params = pup->parameters;
    *outCount = params.size();
    if (!buffer) return NjgResult::Ok;
    if (bufferLength < params.size()) return NjgResult::InvalidArgument;
    // Keep copied names alive for C ABI output.
    thread_local std::vector<std::string> nameCache;
    nameCache.clear();
    nameCache.reserve(params.size());
    for (const auto& p : params) nameCache.push_back(p->name);

    for (std::size_t i = 0; i < params.size(); ++i) {
        const auto& param = params[i];
        buffer[i].uuid = param->uuid;
        buffer[i].isVec2 = param->isVec2;
        buffer[i].min = param->min;
        buffer[i].max = param->max;
        buffer[i].defaults = param->defaults;
        buffer[i].name = nameCache[i].c_str();
        buffer[i].nameLength = nameCache[i].size();
    }
    return NjgResult::Ok;
}

NjgResult njgUpdateParameters(void* puppetHandle, const PuppetParameterUpdate* updates, size_t updateCount) {
    if (!puppetHandle) return NjgResult::InvalidArgument;
    if (!updates || updateCount == 0) return NjgResult::Ok;
    std::shared_ptr<Puppet> pup;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        auto it = gPuppets.find(puppetHandle);
        if (it == gPuppets.end() || !it->second || !it->second->puppet) return NjgResult::InvalidArgument;
        pup = it->second->puppet;
    }
    for (std::size_t i = 0; i < updateCount; ++i) {
        const auto& upd = updates[i];
        if (auto param = pup->findParameter(upd.parameterUuid)) {
            if (param->isVec2) {
                param->value = upd.value;
            } else {
                param->value.x = upd.value.x;
            }
        }
    }
    return NjgResult::Ok;
}

NjgResult njgGetPuppetExtData(void* puppetHandle, const char* key, const uint8_t** outData, size_t* outLength) {
    if (!puppetHandle || !key || !outData || !outLength) return NjgResult::InvalidArgument;
    *outData = nullptr;
    *outLength = 0;

    std::shared_ptr<Puppet> pup;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        auto it = gPuppets.find(puppetHandle);
        if (it == gPuppets.end() || !it->second || !it->second->puppet) return NjgResult::InvalidArgument;
        pup = it->second->puppet;
    }

    auto found = pup->extData.find(key);
    if (found == pup->extData.end() || found->second.empty()) return NjgResult::Failure;
    thread_local std::vector<uint8_t> extScratch;
    extScratch = found->second;
    *outData = extScratch.data();
    *outLength = extScratch.size();
    return NjgResult::Ok;
}

NjgResult njgPlayAnimation(void* renderer, void* puppetHandle, const char* name, bool loop, bool playLeadOut) {
    if (!renderer || !puppetHandle || !name) return NjgResult::InvalidArgument;
    std::shared_ptr<Puppet> pup;
    RendererCtx* rendererCtx = nullptr;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        auto rit = gRenderers.find(renderer);
        if (rit == gRenderers.end()) return NjgResult::InvalidArgument;
        rendererCtx = rit->second.get();
        auto it = gPuppets.find(puppetHandle);
        if (it == gPuppets.end() || !it->second || !it->second->puppet) return NjgResult::InvalidArgument;
        pup = it->second->puppet;
    }
    if (pup->animations.find(name) == pup->animations.end()) {
        unityLog(std::string("[nicxlive] njgPlayAnimation failed: animation not found name=") + name);
        return NjgResult::InvalidArgument;
    }
    auto* state = ensureAnimationState(*rendererCtx, puppetHandle, name);
    if (state->paused) {
        state->paused = false;
    } else {
        state->frame = 0;
        state->looped = 0;
        state->stopping = false;
        state->playing = true;
        state->loop = loop;
        state->playLeadOut = playLeadOut;
        state->paused = false;
    }
    return NjgResult::Ok;
}

NjgResult njgPauseAnimation(void* renderer, void* puppetHandle, const char* name) {
    if (!renderer || !puppetHandle || !name) return NjgResult::InvalidArgument;
    std::shared_ptr<Puppet> pup;
    RendererCtx* rendererCtx = nullptr;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        auto rit = gRenderers.find(renderer);
        if (rit == gRenderers.end()) return NjgResult::InvalidArgument;
        rendererCtx = rit->second.get();
        auto it = gPuppets.find(puppetHandle);
        if (it == gPuppets.end() || !it->second || !it->second->puppet) return NjgResult::InvalidArgument;
        pup = it->second->puppet;
    }
    if (pup->animations.find(name) == pup->animations.end()) {
        unityLog(std::string("[nicxlive] njgPauseAnimation failed: animation not found name=") + name);
        return NjgResult::InvalidArgument;
    }
    auto* state = ensureAnimationState(*rendererCtx, puppetHandle, name);
    state->paused = true;
    return NjgResult::Ok;
}

NjgResult njgStopAnimation(void* renderer, void* puppetHandle, const char* name, bool immediate) {
    if (!renderer || !puppetHandle || !name) return NjgResult::InvalidArgument;
    std::shared_ptr<Puppet> pup;
    RendererCtx* rendererCtx = nullptr;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        auto rit = gRenderers.find(renderer);
        if (rit == gRenderers.end()) return NjgResult::InvalidArgument;
        rendererCtx = rit->second.get();
        auto it = gPuppets.find(puppetHandle);
        if (it == gPuppets.end() || !it->second || !it->second->puppet) return NjgResult::InvalidArgument;
        pup = it->second->puppet;
    }
    if (pup->animations.find(name) == pup->animations.end()) {
        unityLog(std::string("[nicxlive] njgStopAnimation failed: animation not found name=") + name);
        return NjgResult::InvalidArgument;
    }
    auto* state = ensureAnimationState(*rendererCtx, puppetHandle, name);
    if (state->stopping) return NjgResult::Ok;
    const bool shouldStopImmediate = immediate || state->frame == 0 || state->paused || !state->playLeadOut;
    state->stopping = !shouldStopImmediate;
    state->loop = false;
    state->paused = false;
    state->playing = false;
    state->playLeadOut = !shouldStopImmediate;
    if (shouldStopImmediate) {
        state->frame = 0;
        state->looped = 0;
    }
    return NjgResult::Ok;
}

NjgResult njgSeekAnimation(void* renderer, void* puppetHandle, const char* name, int frame) {
    if (!renderer || !puppetHandle || !name) return NjgResult::InvalidArgument;
    std::shared_ptr<Puppet> pup;
    RendererCtx* rendererCtx = nullptr;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        auto rit = gRenderers.find(renderer);
        if (rit == gRenderers.end()) return NjgResult::InvalidArgument;
        rendererCtx = rit->second.get();
        auto it = gPuppets.find(puppetHandle);
        if (it == gPuppets.end() || !it->second || !it->second->puppet) return NjgResult::InvalidArgument;
        pup = it->second->puppet;
    }
    if (pup->animations.find(name) == pup->animations.end()) {
        unityLog(std::string("[nicxlive] njgSeekAnimation failed: animation not found name=") + name);
        return NjgResult::InvalidArgument;
    }
    auto* state = ensureAnimationState(*rendererCtx, puppetHandle, name);
    state->frame = (std::max)(0, frame);
    state->looped = 0;
    return NjgResult::Ok;
}

NjgResult njgSetPuppetScale(void* puppetHandle, float sx, float sy) {
    if (!puppetHandle) return NjgResult::InvalidArgument;
    std::shared_ptr<Puppet> pup;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        auto it = gPuppets.find(puppetHandle);
        if (it == gPuppets.end() || !it->second || !it->second->puppet) return NjgResult::InvalidArgument;
        pup = it->second->puppet;
    }
    pup->transform.scale = nodes::Vec2{sx, sy};
    pup->transform.update();
    if (auto root = pup->actualRoot()) {
        root->transformChanged();
    }
    return NjgResult::Ok;
}

NjgResult njgSetPuppetTranslation(void* puppetHandle, float tx, float ty) {
    if (!puppetHandle) return NjgResult::InvalidArgument;
    std::shared_ptr<Puppet> pup;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        auto it = gPuppets.find(puppetHandle);
        if (it == gPuppets.end() || !it->second || !it->second->puppet) return NjgResult::InvalidArgument;
        pup = it->second->puppet;
    }
    pup->transform.translation.x = tx;
    pup->transform.translation.y = ty;
    pup->transform.update();
    if (auto root = pup->actualRoot()) {
        root->transformChanged();
    }
    return NjgResult::Ok;
}

NjgResult njgUnloadPuppet(void* renderer, void* puppet) {
    if (renderer) {
        std::lock_guard<std::mutex> lock(gMutex);
        auto rit = gRenderers.find(renderer);
        if (rit != gRenderers.end()) {
            auto& v = rit->second->puppetHandles;
            v.erase(std::remove(v.begin(), v.end(), puppet), v.end());
            auto pit = gPuppets.find(puppet);
            if (pit != gPuppets.end() && pit->second && pit->second->puppet) {
                releasePuppetTextures(*rit->second, pit->second->puppet);
            }
            rit->second->animationStates.erase(puppet);
        }
    }
    destroyHandle(gPuppets, puppet);
    return NjgResult::Ok;
}

NjgResult njgBeginFrame(void* renderer, const FrameConfig* cfg) {
    if (!renderer || !cfg) return NjgResult::InvalidArgument;
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gRenderers.find(renderer);
    if (it == gRenderers.end()) return NjgResult::InvalidArgument;
    inSetViewport(cfg->viewportWidth, cfg->viewportHeight);
    // Clear previous frame commands/resources
    it->second->backend->clear();
    it->second->queued.clear();
    // Ensure render/composite targets exist
    if (it->second->callbacks.createTexture && cfg->viewportWidth > 0 && cfg->viewportHeight > 0) {
        bool needRecreate = (it->second->lastViewportW != cfg->viewportWidth) || (it->second->lastViewportH != cfg->viewportHeight);
        if (needRecreate && it->second->renderHandle != 0) {
            releaseExternalTexture(*it->second, it->second->renderHandle);
            it->second->renderHandle = 0;
        }
        if (needRecreate && it->second->compositeHandle != 0) {
            releaseExternalTexture(*it->second, it->second->compositeHandle);
            it->second->compositeHandle = 0;
        }
        if (it->second->renderHandle == 0) {
            it->second->renderHandle = it->second->callbacks.createTexture(cfg->viewportWidth, cfg->viewportHeight,
                                                                           4, 1, 4, /*renderTarget*/ true, false,
                                                                           it->second->callbacks.userData);
            it->second->stats.created++;
            it->second->stats.current++;
        }
        if (it->second->compositeHandle == 0) {
            it->second->compositeHandle = it->second->callbacks.createTexture(cfg->viewportWidth, cfg->viewportHeight,
                                                                              4, 1, 4, /*renderTarget*/ true, false,
                                                                              it->second->callbacks.userData);
            it->second->stats.created++;
            it->second->stats.current++;
        }
        it->second->backend->setRenderTargets(it->second->renderHandle, it->second->compositeHandle);
        it->second->lastViewportW = cfg->viewportWidth;
        it->second->lastViewportH = cfg->viewportHeight;
    }
    setCurrentRenderBackend(it->second->backend);
    return NjgResult::Ok;
}

NjgResult njgTickPuppet(void* puppet, double deltaSeconds) {
    auto profile = render::profileScope("Unity.njgTickPuppet");
    const auto start = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gPuppets.find(puppet);
    if (it == gPuppets.end()) return NjgResult::InvalidArgument;
    for (auto& rendererPair : gRenderers) {
        auto* renderer = rendererPair.second.get();
        auto pit = renderer->animationStates.find(puppet);
        if (pit == renderer->animationStates.end()) continue;
        for (auto& animPair : pit->second) {
            auto& state = animPair.second;
            if (!state.playing || state.paused) continue;
            state.frame += (std::max)(1, static_cast<int>(deltaSeconds * 60.0));
        }
    }
    if (it->second && it->second->puppet) {
        if (std::isfinite(deltaSeconds) && deltaSeconds > 0.0) {
            gUnityTimeTicker += deltaSeconds;
        }
        inUpdate();
        NJCX_DBG_LOG("[nicxlive] tick update start\n");
        it->second->puppet->update();
        NJCX_DBG_LOG("[nicxlive] tick update end graphEmpty=%d rootParts=%zu\n", it->second->puppet->isRenderGraphEmpty() ? 1 : 0, it->second->puppet->rootPartCount());
    }
    unityPerfWindow().addTick(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
    return NjgResult::Ok;
}
NjgResult njgEmitCommands(void* renderer, CommandQueueView* outView) {
    if (!renderer || !outView) return NjgResult::InvalidArgument;
    auto profile = render::profileScope("Unity.njgEmitCommands");
    const auto start = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gRenderers.find(renderer);
    if (it == gRenderers.end()) return NjgResult::InvalidArgument;
    auto& ctx = *it->second;
    ctx.backend->clear();
    setCurrentRenderBackend(ctx.backend);
    // draw all puppets into queue
    NJCX_DBG_LOG("[nicxlive] emit begin puppets=%zu\n", ctx.puppetHandles.size());
    NJCX_DBG_CODE(unityLog(std::string("[nicxlive] emit begin: puppets=") + std::to_string(ctx.puppetHandles.size())););
    for (auto h : ctx.puppetHandles) {
        auto pit = gPuppets.find(h);
        if (pit == gPuppets.end()) continue;
        if (pit->second && pit->second->puppet) {
            pit->second->puppet->draw();
            NJCX_DBG_CODE(unityLog(std::string("[nicxlive] emit draw puppet: handle=") + std::to_string(reinterpret_cast<uintptr_t>(h)) + std::string(" queue=") + std::to_string(ctx.backend->queue.size()) + std::string(" graphEmpty=") + (pit->second->puppet->isRenderGraphEmpty() ? "true" : "false") + std::string(" rootParts=") + std::to_string(pit->second->puppet->rootPartCount())););
            NJCX_DBG_LOG("[nicxlive] emit drew puppet queue=%zu\n", ctx.backend->queue.size());
        }
            NJCX_DBG_LOG("[nicxlive] emit puppet state graphEmpty=%d rootParts=%zu\n", pit->second->puppet->isRenderGraphEmpty() ? 1 : 0, pit->second->puppet->rootPartCount());
    }
    // Apply deferred texture create/update/dispose callbacks.
    double textureCmdMs = 0.0;
    {
        auto applyTextureProfile = render::profileScope("Unity.applyTextureCommands");
        const auto textureStart = std::chrono::steady_clock::now();
        applyTextureCommands(ctx);
        textureCmdMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - textureStart).count();
    }

    const auto packStart = std::chrono::steady_clock::now();
    packQueuedCommands(ctx);
    const double packMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - packStart).count();
    NJCX_DBG_CODE(unityLog(std::string("[nicxlive] emit packed queue=") + std::to_string(ctx.backend->queue.size())););
    outView->commands = ctx.queued.empty() ? nullptr : ctx.queued.data();
    NJCX_DBG_LOG("[nicxlive] emit packed queue=%zu out=%zu\n", ctx.backend->queue.size(), outView->count);
    outView->count = ctx.queued.size();
    NJCX_DBG_CODE(unityLog(std::string("[nicxlive] emit out count=") + std::to_string(outView->count)););
    unityPerfWindow().addEmit(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count(),
        packMs,
        textureCmdMs);
    return NjgResult::Ok;
}

NjgResult njgGetSharedBuffers(void* renderer, SharedBufferSnapshot* snapshot) {
    if (!renderer || !snapshot) return NjgResult::InvalidArgument;
    auto profile = render::profileScope("Unity.njgGetSharedBuffers");
    const auto start = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gRenderers.find(renderer);
    if (it == gRenderers.end()) return NjgResult::InvalidArgument;
    auto vRaw = ::nicxlive::core::render::sharedVertexBufferData().rawStorage();
    auto uvRaw = ::nicxlive::core::render::sharedUvBufferData().rawStorage();
    auto dRaw = ::nicxlive::core::render::sharedDeformBufferData().rawStorage();
    snapshot->vertices = {vRaw.ptr, vRaw.length};
    snapshot->uvs = {uvRaw.ptr, uvRaw.length};
    snapshot->deform = {dRaw.ptr, dRaw.length};
    snapshot->vertexCount = ::nicxlive::core::render::sharedVertexBufferData().size();
    snapshot->uvCount = ::nicxlive::core::render::sharedUvBufferData().size();
    snapshot->deformCount = ::nicxlive::core::render::sharedDeformBufferData().size();
    unityPerfWindow().addShared(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
    return NjgResult::Ok;
}

NjgResult njgGetSharedBufferState(void* renderer, SharedBufferState* state) {
    if (!renderer || !state) return NjgResult::InvalidArgument;
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gRenderers.find(renderer);
    if (it == gRenderers.end()) return NjgResult::InvalidArgument;
    state->vertexRevision = ::nicxlive::core::render::sharedVertexBufferRevision();
    state->uvRevision = ::nicxlive::core::render::sharedUvBufferRevision();
    state->deformRevision = ::nicxlive::core::render::sharedDeformBufferRevision();
    return NjgResult::Ok;
}

NjgRenderTargets njgGetRenderTargets(void* renderer) {
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gRenderers.find(renderer);
    if (it == gRenderers.end()) return NjgRenderTargets{0, 0, 0, 0, 0};
    return NjgRenderTargets{
        it->second->renderHandle,
        it->second->compositeHandle,
        0,
        it->second->lastViewportW,
        it->second->lastViewportH,
    };
}

void njgSetLogCallback(NjgLogFn callback, void* userData) {
    std::lock_guard<std::mutex> lock(gMutex);
    gLogCallback = callback;
    gLogUserData = userData;
}

void njgFlushCommandBuffer(void* renderer) {
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gRenderers.find(renderer);
    if (it == gRenderers.end()) return;
    it->second->queued.clear();
}

size_t njgGetGcHeapSize() {
    // D exposes GC.usedSize; C++ runtime has no equivalent GC heap metric.
    return 0;
}

TextureStats njgGetTextureStats(void* renderer) {
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gRenderers.find(renderer);
    if (it == gRenderers.end()) return TextureStats{0, 0, 0};
    return it->second->stats;
}

NjgResult njgGetWasmLayout(NjgWasmLayout* outLayout) {
    if (!outLayout) return NjgResult::InvalidArgument;

    NjgWasmLayout out{};
    out.sizeQueued = static_cast<uint32_t>(sizeof(NjgQueuedCommand));
    out.offQueuedPart = static_cast<uint32_t>(offsetof(NjgQueuedCommand, partPacket));
    out.offQueuedMaskApply = static_cast<uint32_t>(offsetof(NjgQueuedCommand, maskApplyPacket));
    out.offQueuedDynamic = static_cast<uint32_t>(offsetof(NjgQueuedCommand, dynamicPass));
    out.offQueuedUsesStencil = static_cast<uint32_t>(offsetof(NjgQueuedCommand, usesStencil));

    out.sizePart = static_cast<uint32_t>(sizeof(NjgPartDrawPacket));
    out.offPartTextureHandles = static_cast<uint32_t>(offsetof(NjgPartDrawPacket, textureHandles));
    out.offPartTextureCount = static_cast<uint32_t>(offsetof(NjgPartDrawPacket, textureCount));
    out.offPartOrigin = static_cast<uint32_t>(offsetof(NjgPartDrawPacket, origin));
    out.offPartVertexOffset = static_cast<uint32_t>(offsetof(NjgPartDrawPacket, vertexOffset));
    out.offPartVertexStride = static_cast<uint32_t>(offsetof(NjgPartDrawPacket, vertexAtlasStride));
    out.offPartUvOffset = static_cast<uint32_t>(offsetof(NjgPartDrawPacket, uvOffset));
    out.offPartUvStride = static_cast<uint32_t>(offsetof(NjgPartDrawPacket, uvAtlasStride));
    out.offPartDeformOffset = static_cast<uint32_t>(offsetof(NjgPartDrawPacket, deformOffset));
    out.offPartDeformStride = static_cast<uint32_t>(offsetof(NjgPartDrawPacket, deformAtlasStride));
    out.offPartIndexHandle = static_cast<uint32_t>(offsetof(NjgPartDrawPacket, indexHandle));
    out.offPartIndicesPtr = static_cast<uint32_t>(offsetof(NjgPartDrawPacket, indices));
    out.offPartIndexCount = static_cast<uint32_t>(offsetof(NjgPartDrawPacket, indexCount));
    out.offPartVertexCount = static_cast<uint32_t>(offsetof(NjgPartDrawPacket, vertexCount));

    out.sizeMaskDraw = static_cast<uint32_t>(sizeof(NjgMaskDrawPacket));
    out.offMaskDrawIndicesPtr = static_cast<uint32_t>(offsetof(NjgMaskDrawPacket, indices));
    out.offMaskDrawIndexCount = static_cast<uint32_t>(offsetof(NjgMaskDrawPacket, indexCount));
    out.offMaskDrawVertexOffset = static_cast<uint32_t>(offsetof(NjgMaskDrawPacket, vertexOffset));
    out.offMaskDrawVertexStride = static_cast<uint32_t>(offsetof(NjgMaskDrawPacket, vertexAtlasStride));
    out.offMaskDrawDeformOffset = static_cast<uint32_t>(offsetof(NjgMaskDrawPacket, deformOffset));
    out.offMaskDrawDeformStride = static_cast<uint32_t>(offsetof(NjgMaskDrawPacket, deformAtlasStride));
    out.offMaskDrawIndexHandle = static_cast<uint32_t>(offsetof(NjgMaskDrawPacket, indexHandle));
    out.offMaskDrawVertexCount = static_cast<uint32_t>(offsetof(NjgMaskDrawPacket, vertexCount));

    out.sizeMaskApply = static_cast<uint32_t>(sizeof(NjgMaskApplyPacket));
    out.offMaskKind = static_cast<uint32_t>(offsetof(NjgMaskApplyPacket, kind));
    out.offMaskIsDodge = static_cast<uint32_t>(offsetof(NjgMaskApplyPacket, isDodge));
    out.offMaskPartPacket = static_cast<uint32_t>(offsetof(NjgMaskApplyPacket, partPacket));
    out.offMaskMaskPacket = static_cast<uint32_t>(offsetof(NjgMaskApplyPacket, maskPacket));

    out.sizeDynamicPass = static_cast<uint32_t>(sizeof(NjgDynamicCompositePass));
    out.offDynTextures = static_cast<uint32_t>(offsetof(NjgDynamicCompositePass, textures));
    out.offDynTextureCount = static_cast<uint32_t>(offsetof(NjgDynamicCompositePass, textureCount));
    out.offDynStencil = static_cast<uint32_t>(offsetof(NjgDynamicCompositePass, stencil));
    out.offDynScale = static_cast<uint32_t>(offsetof(NjgDynamicCompositePass, scale));
    out.offDynRotationZ = static_cast<uint32_t>(offsetof(NjgDynamicCompositePass, rotationZ));
    out.offDynAutoScaled = static_cast<uint32_t>(offsetof(NjgDynamicCompositePass, autoScaled));
    out.offDynOrigBuffer = static_cast<uint32_t>(offsetof(NjgDynamicCompositePass, origBuffer));
    out.offDynOrigViewport = static_cast<uint32_t>(offsetof(NjgDynamicCompositePass, origViewport));
    out.offDynDrawBufferCount = static_cast<uint32_t>(offsetof(NjgDynamicCompositePass, drawBufferCount));
    out.offDynHasStencil = static_cast<uint32_t>(offsetof(NjgDynamicCompositePass, hasStencil));

    *outLayout = out;
    return NjgResult::Ok;
}

} // extern "C"
