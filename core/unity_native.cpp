// unity_native: C API bridging for Unity plugin (queue backend path)

#include "unity_native.hpp"

#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
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
#include "render/shared_deform_buffer.hpp"

using namespace nicxlive::core;
using namespace nicxlive::core::render;

namespace {
bool traceCompositeTexEnabled() {
    static int enabled = -1;
    if (enabled >= 0) return enabled != 0;
    const char* v = std::getenv("NJCX_TRACE_COMPOSITE_TEX");
    if (!v) {
        enabled = 0;
        return false;
    }
    enabled = (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 || std::strcmp(v, "TRUE") == 0) ? 1 : 0;
    return enabled != 0;
}

bool traceCompositePacketEnabled() {
    static int enabled = -1;
    if (enabled >= 0) return enabled != 0;
    const char* v = std::getenv("NJCX_TRACE_COMPOSITE_PACKET");
    if (!v) {
        enabled = 0;
        return false;
    }
    enabled = (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 || std::strcmp(v, "TRUE") == 0) ? 1 : 0;
    return enabled != 0;
}

bool isCompositeEyeCandidate(uint32_t vo) {
    switch (vo) {
    case 1169: case 1135: case 1087: case 1063: case 1032: case 977: case 920:
    case 1567: case 1583: case 1587: case 1508: case 1527: case 1531:
        return true;
    default:
        return false;
    }
}

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

    if (!ctx.callbacks.createTexture) {
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

    if (ctx.callbacks.updateTexture) {
        const auto& data = tex->data();
        if (!data.empty()) {
            ctx.callbacks.updateTexture(handle, data.data(), data.size(), w, h, c, ctx.callbacks.userData);
        }
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
        out.partPacket.renderMatrix = pp.renderMatrix;
        out.partPacket.renderRotation = pp.renderRotation;
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
        out.partPacket.indexHandle = pp.indexBuffer;
        auto idxBuf = ctx.backend->getDrawableIndices(pp.indexBuffer);
        if (idxBuf && !idxBuf->empty()) {
            out.partPacket.indexCount = std::min<std::size_t>(pp.indexCount, idxBuf->size());
            out.partPacket.indices = idxBuf->data();
            out.partPacket.vertexCount = pp.vertexCount;
        } else {
            // Queue backend map may miss some late-built quads (e.g. composite self draw).
            // Fall back to packet-local indices in that case.
            out.partPacket.indexCount = pp.indexCount;
            out.partPacket.vertexCount = pp.vertexCount;
            out.partPacket.indices = pp.indices.empty() ? nullptr : pp.indices.data();
        }
        const size_t texCount = (qc.kind == RenderCommandKind::DrawPart) ? 3 : 0;
        for (size_t ti = 0; ti < texCount; ++ti) {
            const auto t = pp.textureUUIDs[ti];
            auto tex = pp.textures[ti];
            size_t h = 0;
            if (t != 0) {
                auto hit = ctx.runtimeTextureHandles.find(t);
                if (hit != ctx.runtimeTextureHandles.end()) h = hit->second;
            }
            if (tex) {
                const uint32_t bid = tex->backendId();
                if (h == 0 && bid != 0) {
                    auto bit = ctx.backendTextureHandles.find(bid);
                    if (bit != ctx.backendTextureHandles.end()) h = bit->second;
                }
            }
            out.partPacket.textureHandles[ti] = h;
        }
        out.partPacket.textureCount = texCount;
        if (traceCompositeTexEnabled() && qc.kind == RenderCommandKind::DrawPart) {
            const uint32_t vo = pp.vertexOffset;
            const bool compositeChildCandidate = isCompositeEyeCandidate(vo);
            if (compositeChildCandidate) {
                std::fprintf(stderr,
                             "[nicxlive] pack-draw vo=%u texUUID=(%u,%u,%u) texHandle=(%zu,%zu,%zu)\n",
                             vo,
                             pp.textureUUIDs[0], pp.textureUUIDs[1], pp.textureUUIDs[2],
                             out.partPacket.textureHandles[0], out.partPacket.textureHandles[1], out.partPacket.textureHandles[2]);
            }
        }
        if (traceCompositePacketEnabled() && qc.kind == RenderCommandKind::DrawPart && isCompositeEyeCandidate(pp.vertexOffset)) {
            static int traceCount = 0;
            if (traceCount < 80) {
                const uint16_t* idxPtr = nullptr;
                std::size_t idxAvail = 0;
                const char* idxSource = "none";
                if (idxBuf && !idxBuf->empty()) {
                    idxPtr = idxBuf->data();
                    idxAvail = idxBuf->size();
                    idxSource = "backendMap";
                } else if (!pp.indices.empty()) {
                    idxPtr = pp.indices.data();
                    idxAvail = pp.indices.size();
                    idxSource = "packetCopy";
                }
                const auto& vbuf = ::nicxlive::core::render::sharedVertexBufferData();
                const auto& ubuf = ::nicxlive::core::render::sharedUvBufferData();
                const auto& dbuf = ::nicxlive::core::render::sharedDeformBufferData();
                const std::size_t vo = pp.vertexOffset;
                const std::size_t uo = pp.uvOffset;
                const std::size_t dfo = pp.deformOffset;
                const std::size_t vc = pp.vertexCount;
                const bool vInRange = (vo + vc) <= vbuf.size();
                const bool uInRange = (uo + vc) <= ubuf.size();
                const bool dInRange = (dfo + vc) <= dbuf.size();
                const bool idxInRange = (idxPtr != nullptr) && (pp.indexCount <= idxAvail);
                const float vx0 = vInRange && vc > 0 ? vbuf.xAt(vo) : 0.0f;
                const float vy0 = vInRange && vc > 0 ? vbuf.yAt(vo) : 0.0f;
                const float ux0 = uInRange && vc > 0 ? ubuf.xAt(uo) : 0.0f;
                const float uy0 = uInRange && vc > 0 ? ubuf.yAt(uo) : 0.0f;
                const float dx0 = dInRange && vc > 0 ? dbuf.xAt(dfo) : 0.0f;
                const float dy0 = dInRange && vc > 0 ? dbuf.yAt(dfo) : 0.0f;
                const unsigned i0 = idxInRange && pp.indexCount > 0 ? idxPtr[0] : 0;
                const unsigned i1 = idxInRange && pp.indexCount > 1 ? idxPtr[1] : 0;
                const unsigned i2 = idxInRange && pp.indexCount > 2 ? idxPtr[2] : 0;
                std::size_t tex0DataLen = 0;
                unsigned tex0First = 0;
                if (pp.textures[0]) {
                    const auto& texData = pp.textures[0]->data();
                    tex0DataLen = texData.size();
                    if (!texData.empty()) tex0First = texData[0];
                }
                std::fprintf(stderr,
                    "[nicxlive][cmp-pack] vo=%u uo=%u do=%u vc=%u idxH=%u idxC=%u idxSrc=%s idxAvail=%zu idxOK=%d i0..2=(%u,%u,%u) "
                    "vStridePkt=%u uStridePkt=%u dStridePkt=%u vStrideBuf=%zu uStrideBuf=%zu dStrideBuf=%zu inRange(v/u/d)=(%d,%d,%d) "
                    "sample v0=(%.6f,%.6f) uv0=(%.6f,%.6f) d0=(%.6f,%.6f) tex0Data=%zu tex0First=%u flags(r=%d m=%d um=%d eb=%d rr=%.6f bm=%d op=%.6f)\n",
                    pp.vertexOffset, pp.uvOffset, pp.deformOffset, pp.vertexCount,
                    pp.indexBuffer, pp.indexCount, idxSource, idxAvail, idxInRange ? 1 : 0, i0, i1, i2,
                    pp.vertexAtlasStride, pp.uvAtlasStride, pp.deformAtlasStride,
                    vbuf.size(), ubuf.size(), dbuf.size(),
                    vInRange ? 1 : 0, uInRange ? 1 : 0, dInRange ? 1 : 0,
                    vx0, vy0, ux0, uy0, dx0, dy0, tex0DataLen, tex0First,
                    pp.renderable ? 1 : 0,
                    pp.isMask ? 1 : 0,
                    pp.useMultistageBlend ? 1 : 0,
                    pp.hasEmissionOrBumpmap ? 1 : 0,
                    pp.renderRotation,
                    static_cast<int>(pp.blendMode),
                    pp.opacity);
                ++traceCount;
            }
        }
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
        maskPart.isMask = applyPart.isMask;
        maskPart.renderable = applyPart.renderable;
        maskPart.modelMatrix = applyPart.modelMatrix;
        maskPart.renderMatrix = applyPart.renderMatrix;
        maskPart.renderRotation = applyPart.renderRotation;
        maskPart.clampedTint = applyPart.clampedTint;
        maskPart.clampedScreen = applyPart.clampedScreen;
        maskPart.opacity = applyPart.opacity;
        maskPart.emissionStrength = applyPart.emissionStrength;
        maskPart.maskThreshold = applyPart.maskThreshold;
        maskPart.blendingMode = static_cast<int>(applyPart.blendMode);
        maskPart.useMultistageBlend = applyPart.useMultistageBlend;
        maskPart.hasEmissionOrBumpmap = applyPart.hasEmissionOrBumpmap;
        maskPart.origin = applyPart.origin;
        maskPart.vertexOffset = applyPart.vertexOffset;
        maskPart.vertexAtlasStride = applyPart.vertexAtlasStride;
        maskPart.uvOffset = applyPart.uvOffset;
        maskPart.uvAtlasStride = applyPart.uvAtlasStride;
        maskPart.deformOffset = applyPart.deformOffset;
        maskPart.deformAtlasStride = applyPart.deformAtlasStride;
        maskPart.indexHandle = applyPart.indexBuffer;
        for (size_t ti = 0; ti < 3; ++ti) {
            const auto t = applyPart.textureUUIDs[ti];
            auto tex = applyPart.textures[ti];
            size_t h = 0;
            if (t != 0) {
                auto hit = ctx.runtimeTextureHandles.find(t);
                if (hit != ctx.runtimeTextureHandles.end()) h = hit->second;
            }
            if (tex) {
                const uint32_t bid = tex->backendId();
                if (h == 0 && bid != 0) {
                    auto bit = ctx.backendTextureHandles.find(bid);
                    if (bit != ctx.backendTextureHandles.end()) h = bit->second;
                }
            }
            maskPart.textureHandles[ti] = h;
        }
        maskPart.textureCount = 3;
        auto applyIdx = ctx.backend->getDrawableIndices(applyPart.indexBuffer);
        if (applyIdx && !applyIdx->empty()) {
            maskPart.indexCount = std::min<std::size_t>(applyPart.indexCount, applyIdx->size());
            maskPart.indices = applyIdx->data();
            maskPart.vertexCount = applyPart.vertexCount;
        } else {
            maskPart.indexCount = applyPart.indexCount;
            maskPart.vertexCount = applyPart.vertexCount;
            maskPart.indices = applyPart.indices.empty() ? nullptr : applyPart.indices.data();
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
        std::fprintf(stderr, "[nicxlive] load: root=%p\n", pup->root.get());
        if (pup->root) {
            pup->root->setPuppet(pup);
            pup->root->reconstruct();
            pup->root->finalize();
        }
        if (auto root = pup->actualRoot()) {
            // D parity (nijilive.integration.unity): for dynamic composite offscreen
            // children, ignore puppet transform on all Projectable nodes.
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
        }
        if (pup->root) {
            std::size_t nodeCount = 0;
            std::size_t partCount = 0;
            std::size_t maskCount = 0;
            std::size_t projectableCount = 0;
            std::size_t partWithMaskCount = 0;
            std::size_t partWithTextureIdCount = 0;
            std::size_t partWithTexturePtrCount = 0;
            std::function<void(const std::shared_ptr<nodes::Node>&)> countNodes;
            countNodes = [&](const std::shared_ptr<nodes::Node>& n) {
                if (!n) return;
                ++nodeCount;
                if (auto p = std::dynamic_pointer_cast<nodes::Part>(n)) {
                    ++partCount;
                    if (!p->masks.empty()) ++partWithMaskCount;
                    if (!p->textureIds.empty()) ++partWithTextureIdCount;
                    bool hasTexPtr = false;
                    for (const auto& t : p->textures) {
                        if (t) {
                            hasTexPtr = true;
                            break;
                        }
                    }
                    if (hasTexPtr) ++partWithTexturePtrCount;
                    static int dbgPartLog = 0;
                    if (dbgPartLog < 10) {
                        std::fprintf(stderr, "[nicxlive] part summary uuid=%u texIds=%zu tex0=%d masks=%zu\n",
                                     p->uuid,
                                     p->textureIds.size(),
                                     p->textures[0] ? 1 : 0,
                                     p->masks.size());
                        ++dbgPartLog;
                    }
                }
                if (std::dynamic_pointer_cast<nodes::Mask>(n)) ++maskCount;
                if (std::dynamic_pointer_cast<nodes::Projectable>(n)) ++projectableCount;
                for (const auto& child : n->childrenList()) countNodes(child);
            };
            std::function<void(const std::shared_ptr<nodes::Node>&)> finalizeParts;
            finalizeParts = [&](const std::shared_ptr<nodes::Node>& n) {
                if (!n) return;
                if (auto p = std::dynamic_pointer_cast<nodes::Part>(n)) {
                    p->finalize();
                }
                for (const auto& child : n->childrenList()) finalizeParts(child);
            };
            finalizeParts(pup->root);
            countNodes(pup->root);
            std::fprintf(stderr, "[nicxlive] load tree: rootChildren=%zu nodes=%zu parts=%zu masks=%zu projectables=%zu parts(masked=%zu texIds=%zu texPtrs=%zu\n",
                         pup->root->childrenList().size(),
                         nodeCount,
                         partCount,
                         maskCount,
                         projectableCount,
                         partWithMaskCount,
                         partWithTextureIdCount,
                         partWithTexturePtrCount);
            pup->scanParts(true, pup->root);
            auto root = pup->actualRoot();
            if (root) {
                pup->rescanNodes();
                root->build(true);
            }
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
                std::fprintf(stderr, "[nicxlive] load: puppets=%zu\n", rit->second->puppetHandles.size());
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
}NjgResult njgGetParameters(void* puppetHandle, NjgParameterInfo* buffer, size_t bufferLength, size_t* outCount) {
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
        std::fprintf(stderr, "[nicxlive] tick update start\n");
        it->second->puppet->update();
        std::fprintf(stderr, "[nicxlive] tick update end graphEmpty=%d rootParts=%zu\n", it->second->puppet->isRenderGraphEmpty() ? 1 : 0, it->second->puppet->rootPartCount());
    }
    return NjgResult::Ok;
}
NjgResult njgEmitCommands(void* renderer, CommandQueueView* outView) {
    if (!renderer || !outView) return NjgResult::InvalidArgument;
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gRenderers.find(renderer);
    if (it == gRenderers.end()) return NjgResult::InvalidArgument;
    auto& ctx = *it->second;
    ctx.backend->clear();
    setCurrentRenderBackend(ctx.backend);
    // draw all puppets into queue
    std::fprintf(stderr, "[nicxlive] emit begin puppets=%zu\n", ctx.puppetHandles.size());
    unityLog(std::string("[nicxlive] emit begin: puppets=") + std::to_string(ctx.puppetHandles.size()));
    for (auto h : ctx.puppetHandles) {
        auto pit = gPuppets.find(h);
        if (pit == gPuppets.end()) continue;
        if (pit->second && pit->second->puppet) {
            pit->second->puppet->draw();
            unityLog(std::string("[nicxlive] emit draw puppet: handle=") + std::to_string(reinterpret_cast<uintptr_t>(h)) + std::string(" queue=") + std::to_string(ctx.backend->queue.size()) + std::string(" graphEmpty=") + (pit->second->puppet->isRenderGraphEmpty() ? "true" : "false") + std::string(" rootParts=") + std::to_string(pit->second->puppet->rootPartCount()));
            std::fprintf(stderr, "[nicxlive] emit drew puppet queue=%zu\n", ctx.backend->queue.size());
        }
            std::fprintf(stderr, "[nicxlive] emit puppet state graphEmpty=%d rootParts=%zu\n", pit->second->puppet->isRenderGraphEmpty() ? 1 : 0, pit->second->puppet->rootPartCount());
    }
    // Apply deferred texture create/update/dispose callbacks.
    applyTextureCommands(ctx);

    packQueuedCommands(ctx);
    unityLog(std::string("[nicxlive] emit packed queue=") + std::to_string(ctx.backend->queue.size()));
    outView->commands = ctx.queued.empty() ? nullptr : ctx.queued.data();
    std::fprintf(stderr, "[nicxlive] emit packed queue=%zu out=%zu\n", ctx.backend->queue.size(), outView->count);
    outView->count = ctx.queued.size();
    unityLog(std::string("[nicxlive] emit out count=") + std::to_string(outView->count));
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
