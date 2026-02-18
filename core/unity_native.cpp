// unity_native: C API bridging for Unity plugin (queue backend path)

#include "unity_native.hpp"

#include <mutex>
#include <unordered_map>
#include <algorithm>
#include <string>
#if defined(_WIN32)
#include <malloc.h>
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "Psapi.lib")
#elif defined(__APPLE__)
#include <malloc/malloc.h>
#endif
#include <cstring>
#include "runtime_state.hpp"
#include "timing.hpp"
#include "render/shared_deform_buffer.hpp"

using namespace nicxlive::core;
using namespace nicxlive::core::render;

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
    std::unordered_map<uint32_t, size_t> externalTextureHandles{};
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
            if (!ctx.callbacks.createTexture) break;
            size_t handle = ctx.callbacks.createTexture(rc.width, rc.height, rc.inChannels, 1, rc.outChannels,
                                                        /*renderTarget*/ false, rc.stencil, ctx.callbacks.userData);
            ctx.externalTextureHandles[rc.id] = handle;
            ctx.stats.created++;
            ctx.stats.current++;
            break;
        }
        case TextureCommandKind::Update: {
            auto it = ctx.externalTextureHandles.find(rc.id);
            if (it == ctx.externalTextureHandles.end()) break;
            if (ctx.callbacks.updateTexture) {
                ctx.callbacks.updateTexture(it->second, rc.data.data(), rc.data.size(), rc.width, rc.height, rc.inChannels, ctx.callbacks.userData);
            }
            break;
        }
        case TextureCommandKind::Params:
            // Unity側でのパラメータ設定はAPIがないため無視
            break;
        case TextureCommandKind::Dispose: {
            auto it = ctx.externalTextureHandles.find(rc.id);
            if (it != ctx.externalTextureHandles.end()) {
                if (ctx.callbacks.releaseTexture) {
                    ctx.callbacks.releaseTexture(it->second, ctx.callbacks.userData);
                }
                ctx.externalTextureHandles.erase(it);
                ctx.stats.released++;
                if (ctx.stats.current > 0) ctx.stats.current--;
            }
            break;
        }
        }
    }
    ctx.backend->resourceQueue.clear();
}

static void ensurePuppetTextures(RendererCtx& ctx, const std::shared_ptr<Puppet>& puppet) {
    if (!puppet || !ctx.callbacks.createTexture) return;
    for (const auto& tex : puppet->textureSlots) {
        if (!tex) continue;
        uint32_t uuid = tex->getRuntimeUUID();
        if (uuid == 0) continue;
        if (ctx.externalTextureHandles.find(uuid) != ctx.externalTextureHandles.end()) continue;
        size_t handle = ctx.callbacks.createTexture(tex->width(), tex->height(), tex->channels(), 1, tex->channels(),
                                                    /*renderTarget*/ false, tex->stencil(), ctx.callbacks.userData);
        if (handle != 0) {
            ctx.externalTextureHandles[uuid] = handle;
            const auto& data = tex->data();
            if (!data.empty() && ctx.callbacks.updateTexture) {
                ctx.callbacks.updateTexture(handle, data.data(), data.size(), tex->width(), tex->height(), tex->channels(), ctx.callbacks.userData);
            }
            ctx.stats.created++;
            ctx.stats.current++;
        }
    }
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
        auto it = ctx.externalTextureHandles.find(uuid);
        if (it != ctx.externalTextureHandles.end()) {
            releaseExternalTexture(ctx, it->second);
            ctx.externalTextureHandles.erase(it);
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

static void packQueuedCommands(RendererCtx& ctx) {
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
        out.partPacket.isMask = pp.isMask;
        out.partPacket.renderable = pp.renderable;
        out.partPacket.modelMatrix = pp.modelMatrix;
        out.partPacket.puppetMatrix = pp.puppetMatrix;
        out.partPacket.clampedTint = pp.clampedTint;
        out.partPacket.clampedScreen = pp.clampedScreen;
        out.partPacket.opacity = pp.opacity;
        out.partPacket.emissionStrength = pp.emissionStrength;
        out.partPacket.maskThreshold = pp.maskThreshold;
        out.partPacket.blendingMode = static_cast<int>(pp.blendMode);
        out.partPacket.useMultistageBlend = pp.useMultistageBlend;
        out.partPacket.hasEmissionOrBumpmap = pp.hasEmissionOrBumpmap;
        out.partPacket.origin = pp.origin;
        out.partPacket.vertexOffset = pp.vertexOffset;
        out.partPacket.vertexAtlasStride = pp.vertexAtlasStride;
        out.partPacket.uvOffset = pp.uvOffset;
        out.partPacket.uvAtlasStride = pp.uvAtlasStride;
        out.partPacket.deformOffset = pp.deformOffset;
        out.partPacket.deformAtlasStride = pp.deformAtlasStride;
        const auto* idxBuf = ctx.backend->getDrawableIndices(pp.indexBuffer);
        if (idxBuf && !idxBuf->empty()) {
            out.partPacket.indexCount = std::min<std::size_t>(pp.indexCount, idxBuf->size());
            out.partPacket.indices = idxBuf->data();
            out.partPacket.vertexCount = pp.vertexCount;
        } else {
            out.partPacket.indexCount = 0;
            out.partPacket.vertexCount = 0;
            out.partPacket.indices = nullptr;
        }
        size_t texCount = 0;
        for (auto t : pp.textureUUIDs) {
            if (t != 0 && texCount < 3) {
                auto hit = ctx.externalTextureHandles.find(t);
                out.partPacket.textureHandles[texCount++] = (hit != ctx.externalTextureHandles.end()) ? hit->second : t;
            }
        }
        out.partPacket.textureCount = texCount;

        // Mask apply
        out.maskApplyPacket.kind = static_cast<nicxlive::core::RenderBackend::MaskDrawableKind>(qc.maskApplyPacket.kind);
        out.maskApplyPacket.isDodge = qc.maskApplyPacket.isDodge;
        // part packet for ApplyMask uses the packet embedded in maskApplyPacket
        const auto& applyPart = qc.maskApplyPacket.partPacket;
        const auto* applyIdx = ctx.backend->getDrawableIndices(applyPart.indexBuffer);
        out.maskApplyPacket.partPacket = out.partPacket; // default
        if (applyIdx && !applyIdx->empty()) {
            auto& dst = out.maskApplyPacket.partPacket;
            dst = out.partPacket;
            dst.indexCount = std::min<std::size_t>(applyPart.indexCount, applyIdx->size());
            dst.indices = applyIdx->data();
            dst.vertexCount = applyPart.vertexCount;
        } else if (qc.maskApplyPacket.kind == nicxlive::core::RenderBackend::MaskDrawableKind::Part) {
            out.maskApplyPacket.partPacket.indexCount = 0;
            out.maskApplyPacket.partPacket.vertexCount = 0;
            out.maskApplyPacket.partPacket.indices = nullptr;
        }
        // mask packet from qc.maskApplyPacket.maskPacket
        const auto& mp = qc.maskApplyPacket.maskPacket;
        out.maskApplyPacket.maskPacket.modelMatrix = mp.modelMatrix;
        out.maskApplyPacket.maskPacket.mvp = mp.mvp;
        out.maskApplyPacket.maskPacket.origin = mp.origin;
        out.maskApplyPacket.maskPacket.vertexOffset = mp.vertexOffset;
        out.maskApplyPacket.maskPacket.vertexAtlasStride = mp.vertexAtlasStride;
        out.maskApplyPacket.maskPacket.deformOffset = mp.deformOffset;
        out.maskApplyPacket.maskPacket.deformAtlasStride = mp.deformAtlasStride;
        const auto* midx = ctx.backend->getDrawableIndices(mp.indexBuffer);
        if (midx && !midx->empty()) {
            out.maskApplyPacket.maskPacket.indexCount = std::min<std::size_t>(mp.indexCount, midx->size());
            out.maskApplyPacket.maskPacket.indices = midx->data();
            out.maskApplyPacket.maskPacket.vertexCount = mp.vertexCount;
        } else {
            out.maskApplyPacket.maskPacket.indexCount = 0;
            out.maskApplyPacket.maskPacket.vertexCount = 0;
            out.maskApplyPacket.maskPacket.indices = nullptr;
        }

        // Dynamic pass
        out.dynamicPass.textureCount = qc.dynamicPass.textures.size();
        for (size_t i = 0; i < qc.dynamicPass.textures.size() && i < 3; ++i) {
            out.dynamicPass.textures[i] = qc.dynamicPass.textures[i] ? qc.dynamicPass.textures[i]->backendId() : 0;
        }
        out.dynamicPass.stencil = qc.dynamicPass.stencil ? qc.dynamicPass.stencil->backendId() : 0;
        out.dynamicPass.scale = qc.dynamicPass.scale;
        out.dynamicPass.rotationZ = qc.dynamicPass.rotationZ;
        out.dynamicPass.origBuffer = qc.dynamicPass.origBuffer;
        for (int i = 0; i < 4; ++i) out.dynamicPass.origViewport[i] = qc.dynamicPass.origViewport[i];
        out.usesStencil = qc.usesStencil;

        ctx.queued.push_back(out);
    }
}

extern "C" {

void njgRuntimeInit() {
    std::lock_guard<std::mutex> lock(gMutex);
    if (gRuntimeInitialized) return;
    gRuntimeInitialized = true;
    inSetTimingFunc([]() {
        using namespace std::chrono;
        auto now = steady_clock::now().time_since_epoch();
        return duration_cast<duration<double>>(now).count();
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
        // release remaining external textures
        for (auto& kv : it->second->externalTextureHandles) {
            releaseExternalTexture(*it->second, kv.second);
        }
        it->second->externalTextureHandles.clear();
        it->second->animationStates.clear();
        gRenderers.erase(it);
    }
}

NjgResult njgLoadPuppet(void* renderer, const char* pathUtf8, void** outPuppet) {
    if (!renderer || !pathUtf8 || !outPuppet) return NjgResult::InvalidArgument;
    auto pup = fmt::inLoadPuppet<Puppet>(pathUtf8);
    auto ctx = std::make_unique<PuppetCtx>();
    ctx->puppet = pup;
    void* handle = ctx.get();
    {
        std::lock_guard<std::mutex> lock(gMutex);
        gPuppets[handle] = std::move(ctx);
        auto rit = gRenderers.find(renderer);
        if (rit != gRenderers.end()) {
            rit->second->puppetHandles.push_back(handle);
            ensurePuppetTextures(*rit->second, pup);
            // D版 ensureTextureHandle 相当: backend 側に再生用のテクスチャコマンドを積む
            auto qb = rit->second->backend;
            if (qb) {
                for (const auto& tex : pup->textureSlots) {
                    if (!tex) continue;
                    uint32_t uuid = tex->getRuntimeUUID();
                    if (uuid == 0) continue;
                    qb->createTexture(tex->data(), tex->width(), tex->height(), tex->channels(), tex->stencil());
                    qb->setTextureParams(uuid, ::nicxlive::core::Filtering::Linear, ::nicxlive::core::Wrapping::Clamp, 1.0f);
                }
            }
        }
    }
    *outPuppet = handle;
    return NjgResult::Ok;
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

    // 名前の寿命確保用キャッシュ
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

NjgResult njgTickPuppet(void* puppet, float deltaSeconds) {
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
            state.frame += (std::max)(1, static_cast<int>(deltaSeconds * 60.0f));
        }
    }
    if (it->second && it->second->puppet) {
        inUpdate();
        it->second->puppet->update();
    }
    return NjgResult::Ok;
}

NjgResult njgEmitCommands(void* renderer, SharedBufferSnapshot* shared, const CommandQueueView** outView) {
    if (!renderer || !outView) return NjgResult::InvalidArgument;
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gRenderers.find(renderer);
    if (it == gRenderers.end()) return NjgResult::InvalidArgument;
    auto& ctx = *it->second;
    ctx.backend->clear();
    setCurrentRenderBackend(ctx.backend);
    // draw all puppets into queue
    for (auto h : ctx.puppetHandles) {
        auto pit = gPuppets.find(h);
        if (pit == gPuppets.end()) continue;
        if (pit->second && pit->second->puppet) {
            pit->second->puppet->draw();
        }
    }

    // 外部テクスチャコールバック処理
    applyTextureCommands(ctx);

    packQueuedCommands(ctx);
    static CommandQueueView view{};
    view.commands = ctx.queued.empty() ? nullptr : ctx.queued.data();
    view.count = ctx.queued.size();
    *outView = &view;

    if (shared) {
        auto vRaw = ::nicxlive::core::render::sharedVertexBufferData().rawStorage();
        auto uvRaw = ::nicxlive::core::render::sharedUvBufferData().rawStorage();
        auto dRaw = ::nicxlive::core::render::sharedDeformBufferData().rawStorage();
        shared->vertices = {vRaw.ptr, vRaw.length};
        shared->uvs = {uvRaw.ptr, uvRaw.length};
        shared->deform = {dRaw.ptr, dRaw.length};
        shared->vertexCount = ::nicxlive::core::render::sharedVertexBufferData().size();
        shared->uvCount = ::nicxlive::core::render::sharedUvBufferData().size();
        shared->deformCount = ::nicxlive::core::render::sharedDeformBufferData().size();
    }
    return NjgResult::Ok;
}

NjgResult njgGetSharedBuffers(void* renderer, SharedBufferSnapshot* snapshot) {
    if (!renderer || !snapshot) return NjgResult::InvalidArgument;
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

} // extern "C"
