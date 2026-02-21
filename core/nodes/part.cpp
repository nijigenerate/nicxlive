#include "part.hpp"

#include "../puppet.hpp"
#include "../render.hpp"
#include "../render/commands.hpp"
#include "../render/backend_queue.hpp"
#include "../runtime_state.hpp"
#include "../texture.hpp"
#include "../../fmt/fmt.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>

namespace nicxlive::core::nodes {
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

uint32_t tracePartUuid() {
    static bool init = false;
    static uint32_t value = 0;
    if (init) return value;
    init = true;
    const char* v = std::getenv("NJCX_TRACE_PART_UUID");
    if (!v || *v == '\0') return 0;
    value = static_cast<uint32_t>(std::strtoul(v, nullptr, 10));
    return value;
}
}

static std::vector<MaskBinding> dedupMasks(const std::vector<MaskBinding>& masks);
static void emitMasks(const std::vector<MaskBinding>& bindings, core::RenderCommandEmitter& emitter);

Part::Part() {
    initPartTasks();
}

Part::Part(const MeshData& data, const std::array<std::shared_ptr<::nicxlive::core::Texture>, 3>& tex, uint32_t uuidVal)
    : Drawable(data, uuidVal), textures(tex) {
    if (uuidVal != 0) uuid = uuidVal;
    initPartTasks();
    updateVertices();
    updateUVs();
}

Part::Part(const MeshData& data, const std::vector<std::shared_ptr<::nicxlive::core::Texture>>& tex, uint32_t uuidVal)
    : Part(data, [&]() {
    std::array<std::shared_ptr<::nicxlive::core::Texture>, 3> t{};
    for (std::size_t i = 0; i < t.size() && i < tex.size(); ++i) t[i] = tex[i];
    return t;
}(), uuidVal) {}

std::shared_ptr<Part> Part::createSimple(const std::shared_ptr<::nicxlive::core::Texture>& tex, const std::string& name) {
    if (!tex) return nullptr;
    MeshData data;
    float hw = static_cast<float>(tex->width()) * 0.5f;
    float hh = static_cast<float>(tex->height()) * 0.5f;
    data.vertices = {Vec2{-hw, -hh}, Vec2{-hw, hh}, Vec2{hw, -hh}, Vec2{hw, hh}};
    data.uvs = {Vec2{0, 0}, Vec2{0, 1}, Vec2{1, 0}, Vec2{1, 1}};
    data.indices = {0, 1, 2, 2, 1, 3};
    auto p = std::make_shared<Part>(data, std::array<std::shared_ptr<::nicxlive::core::Texture>, 3>{tex, nullptr, nullptr}, 0);
    p->name = name;
    return p;
}

std::shared_ptr<Part> Part::createSimpleFromFile(const std::string& path, const std::string& name) {
    auto tex = std::make_shared<::nicxlive::core::Texture>(path);
    if (!tex) return nullptr;
    return createSimple(tex, name);
}

std::shared_ptr<Part> Part::createSimpleFromShallow(const ::nicxlive::core::ShallowTexture& shallow, const std::string& name) {
    auto tex = std::make_shared<::nicxlive::core::Texture>(shallow);
    if (!tex) return nullptr;
    return createSimple(tex, name);
}

// Legacy free functions for convenience (D に合わせた API 互換)
std::shared_ptr<Part> inCreateSimplePart(const std::string& file, const std::shared_ptr<Node>& parent = nullptr, const std::string& name = "New Part") {
    auto part = Part::createSimpleFromFile(file, name);
    if (part && parent) parent->addChild(part);
    return part;
}

std::shared_ptr<Part> inCreateSimplePart(const ::nicxlive::core::ShallowTexture& tex, const std::shared_ptr<Node>& parent = nullptr, const std::string& name = "New Part") {
    auto part = Part::createSimpleFromShallow(tex, name);
    if (part && parent) parent->addChild(part);
    return part;
}

std::shared_ptr<Part> inCreateSimplePart(const std::shared_ptr<::nicxlive::core::Texture>& tex, const std::shared_ptr<Node>& parent = nullptr, const std::string& name = "New Part") {
    auto part = Part::createSimple(tex, name);
    if (part && parent) parent->addChild(part);
    return part;
}

const std::string& Part::typeId() const {
    static const std::string k = "Part";
    return k;
}

void Part::initPartTasks() { requireRenderTask(); }

void Part::updateUVs() {
    sharedUvResize(mesh->uvs, mesh->uvs.size());
    sharedUvMarkDirty();
}

void Part::drawSelf(bool /*isMask*/) {
    core::RenderContext ctx;
    ctx.renderBackend = core::getCurrentRenderBackend().get();
    enqueueRenderCommands(ctx);
}

void Part::drawSelfPacket(bool isMask) {
    auto pup = puppetRef();
    if (!pup) return;
    auto backend = core::getCurrentRenderBackend();
    if (!backend) return;
    PartDrawPacket packet{};
    fillDrawPacket(*this, packet, isMask);
    core::RenderCommandEmitter* emitter = pup->commandEmitter();
    if (emitter) {
        emitter->drawPartPacket(packet);
    }
}

void Part::drawOneImmediate() {
    auto backend = core::getCurrentRenderBackend();
    if (!backend) return;
    auto qb = std::dynamic_pointer_cast<core::render::QueueRenderBackend>(backend);
    if (qb) {
        core::render::QueueCommandEmitter emitter(qb);
        auto bindings = dedupMasks(masks);
        if (!bindings.empty()) emitMasks(bindings, emitter);
        emitter.drawPart(std::dynamic_pointer_cast<Part>(shared_from_this()), false);
        if (!bindings.empty()) emitter.endMask();
    } else if (auto unity = std::dynamic_pointer_cast<core::UnityRenderBackend>(backend)) {
        auto bindings = dedupMasks(masks);
        for (auto m : bindings) {
            PartDrawPacket mp{};
            if (m.maskSrc) {
                m.maskSrc->fillDrawPacket(*m.maskSrc, mp, true);
                unity->submitMask(mp);
            }
        }
        PartDrawPacket pkt{};
        fillDrawPacket(*this, pkt, false);
        unity->submitPacket(pkt);
    } else {
        drawSelf(false);
    }
}

// Internal helper to push mask application for current bindings
static void emitMasks(const std::vector<MaskBinding>& bindings, core::RenderCommandEmitter& emitter) {
    if (bindings.empty()) return;
    bool useStencil = false;
    for (const auto& m : bindings) {
        if (m.mode == MaskingMode::Mask) { useStencil = true; break; }
    }
    emitter.beginMask(useStencil);
    for (auto m : bindings) {
        auto maskPtr = m.maskSrc;
        if (!maskPtr) continue;
        emitter.applyMask(maskPtr, m.mode == MaskingMode::DodgeMask);
    }
    emitter.beginMaskContent();
}

void Part::serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const {
    Drawable::serializeSelfImpl(serializer, recursive, flags);
    if (has_flag(flags, SerializeNodeFlags::State)) {
        serializer.putKey("blend_mode");
        serializer.putValue(static_cast<int>(blendMode));
        serializer.putKey("tint");
        serializer.putValue(std::string{std::to_string(tint.x) + "," + std::to_string(tint.y) + "," + std::to_string(tint.z)});
        serializer.putKey("screenTint");
        serializer.putValue(std::string{std::to_string(screenTint.x) + "," + std::to_string(screenTint.y) + "," + std::to_string(screenTint.z)});
        serializer.putKey("emissionStrength");
        serializer.putValue(emissionStrength);
        serializer.putKey("mask_threshold");
        serializer.putValue(maskAlphaThreshold);
        serializer.putKey("opacity");
        serializer.putValue(opacity);
        serializer.putKey("ignorePuppet");
        serializer.putValue(ignorePuppet);
    }
    if (has_flag(flags, SerializeNodeFlags::Links)) {
        serializer.putKey("textures");
        auto arr = serializer.listBegin();
        if (!textureIds.empty()) {
            for (auto t : textureIds) {
                serializer.elemBegin();
                serializer.putValue(static_cast<std::size_t>(t));
            }
        } else {
            for (const auto& tex : textures) {
                serializer.elemBegin();
                std::size_t id = tex ? static_cast<std::size_t>(tex->getRuntimeUUID()) : static_cast<std::size_t>(NOINDEX);
                serializer.putValue(id);
            }
        }
        serializer.listEnd(arr);
        if (!maskedBy.empty()) {
            serializer.putKey("masked_by");
            auto marr = serializer.listBegin();
            for (auto id : maskedBy) {
                serializer.elemBegin();
                serializer.putValue(static_cast<std::size_t>(id));
            }
            serializer.listEnd(marr);
            serializer.putKey("mask_mode");
            serializer.putValue(static_cast<int>(maskedByMode));
        }
        if (!maskList.empty()) {
            serializer.putKey("maskList");
            auto marr = serializer.listBegin();
            for (const auto& l : maskList) {
                serializer.elemBegin();
                serializer.putKey("target");
                serializer.putValue(static_cast<std::size_t>(l.target));
            }
            serializer.listEnd(marr);
        }
    }
    if (has_flag(flags, SerializeNodeFlags::Links) && !masks.empty()) {
        serializer.putKey("masks");
        auto arr = serializer.listBegin();
        for (const auto& m : masks) {
            serializer.elemBegin();
            serializer.putKey("source");
            serializer.putValue(m.maskSrcUUID);
            serializer.putKey("mode");
            serializer.putValue(static_cast<int>(m.mode));
        }
        serializer.listEnd(arr);
    }
}

::nicxlive::core::serde::SerdeException Part::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    if (auto err = Drawable::deserializeFromFghj(data)) return err;
    if (auto bm = data.get_optional<int>("blend_mode")) {
        if (*bm >= 0 && *bm <= static_cast<int>(BlendMode::SliceFromLower)) {
            blendMode = static_cast<BlendMode>(*bm);
        }
    } else if (auto bs = data.get_optional<std::string>("blend_mode")) {
        if (auto parsed = parseBlendMode(*bs)) {
            blendMode = *parsed;
        }
    }
    if (auto th = data.get_optional<float>("mask_threshold")) maskAlphaThreshold = *th;
    if (auto op = data.get_optional<float>("opacity")) opacity = *op;
    if (auto em = data.get_optional<float>("emissionStrength")) emissionStrength = *em;
    if (auto ign = data.get_optional<bool>("ignorePuppet")) ignorePuppet = *ign;
    if (auto ti = data.get_optional<std::string>("tint")) {
        std::stringstream ss(*ti);
        char comma;
        if (ss >> tint.x >> comma >> tint.y >> comma >> tint.z) {
        }
    }
    if (auto st = data.get_optional<std::string>("screenTint")) {
        std::stringstream ss(*st);
        char comma;
        if (ss >> screenTint.x >> comma >> screenTint.y >> comma >> screenTint.z) {
        }
    }
    if (auto ml = data.get_child_optional("maskList")) {
        maskList.clear();
        for (const auto& elem : *ml) {
            MaskLink l;
            l.target = elem.second.get<uint32_t>("target", 0);
            maskList.push_back(l);
        }
    }
    textureIds.clear();
    if (auto tx = data.get_child_optional("textures")) {
        // Keep texture slots compact like D implementation:
        // skip NO_TEXTURE sentinels instead of reserving empty entries.
        constexpr uint32_t kNoTexture = std::numeric_limits<uint32_t>::max();
        for (const auto& e : *tx) {
            const uint32_t raw = static_cast<uint32_t>(e.second.get_value<std::size_t>());
            if (raw == kNoTexture) continue;
            textureIds.push_back(static_cast<int32_t>(raw));
        }
    }
    if (auto mb = data.get_child_optional("masked_by")) {
        for (const auto& e : *mb) {
            maskedBy.push_back(static_cast<uint32_t>(e.second.get_value<std::size_t>()));
        }
    }
    if (auto mm = data.get_optional<int>("mask_mode")) {
        int modeVal = *mm;
        if (modeVal >= 0 && modeVal <= static_cast<int>(MaskingMode::DodgeMask)) {
            maskedByMode = static_cast<MaskingMode>(modeVal);
        }
    }
    if (auto pup = puppetRef()) {
        for (std::size_t i = 0; i < textureIds.size() && i < textures.size(); ++i) {
            uint32_t id = static_cast<uint32_t>(textureIds[i]);
            auto tex = pup->resolveTextureSlot(id);
            if (tex) textures[i] = tex;
        }
    }
    masks.clear();
    if (auto ml = data.get_child_optional("masks")) {
        for (const auto& e : *ml) {
            MaskBinding mb;
            mb.maskSrcUUID = e.second.get<uint32_t>("source", 0);
            int modeVal = e.second.get<int>("mode", 0);
            if (modeVal >= 0 && modeVal <= static_cast<int>(MaskingMode::DodgeMask)) {
                mb.mode = static_cast<MaskingMode>(modeVal);
            }
            masks.push_back(mb);
        }
    }
    if (!maskList.empty()) {
        for (const auto& l : maskList) {
            MaskBinding mb;
            mb.maskSrcUUID = l.target;
            mb.mode = maskedByMode;
            masks.push_back(mb);
        }
    }
    return std::nullopt;
}

void Part::serializePartial(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive) const {
    // Serialize only textures as UUIDs plus basic state for partial save
    serializer.putKey("textureUUIDs");
    auto arr = serializer.listBegin();
    for (const auto& tex : textures) {
        serializer.elemBegin();
        uint32_t id = tex ? tex->getRuntimeUUID() : 0;
        serializer.putValue(static_cast<std::size_t>(id));
    }
    serializer.listEnd(arr);
}

void Part::renderMask(bool dodge) {
    auto backend = core::getCurrentRenderBackend();
    if (!backend) return;
    PartDrawPacket packet{};
    fillDrawPacket(*this, packet, true);
    core::RenderBackend::MaskApplyPacket maskPacket;
    maskPacket.kind = core::RenderBackend::MaskDrawableKind::Part;
    maskPacket.partPacket = packet;
    maskPacket.isDodge = dodge;
    backend->applyMask(maskPacket);
}

bool Part::hasParam(const std::string& key) const {
    if (Drawable::hasParam(key)) return true;
    return key == "alphaThreshold" || key == "opacity" ||
           key == "tint.r" || key == "tint.g" || key == "tint.b" ||
           key == "screenTint.r" || key == "screenTint.g" || key == "screenTint.b" ||
           key == "emissionStrength";
}

float Part::getDefaultValue(const std::string& key) const {
    float def = Drawable::getDefaultValue(key);
    if (!std::isnan(def)) return def;
    if (key == "alphaThreshold") return 0.0f;
    if (key == "opacity" || key == "tint.r" || key == "tint.g" || key == "tint.b") return 1.0f;
    if (key == "screenTint.r" || key == "screenTint.g" || key == "screenTint.b") return 0.0f;
    if (key == "emissionStrength") return 1.0f;
    return std::numeric_limits<float>::quiet_NaN();
}

bool Part::setValue(const std::string& key, float value) {
    if (Drawable::setValue(key, value)) return true;
    if (key == "alphaThreshold") { offsetMaskThreshold *= value; return true; }
    if (key == "opacity") { offsetOpacity *= value; return true; }
    if (key == "tint.r") { offsetTint.x *= value; return true; }
    if (key == "tint.g") { offsetTint.y *= value; return true; }
    if (key == "tint.b") { offsetTint.z *= value; return true; }
    if (key == "screenTint.r") { offsetScreenTint.x += value; return true; }
    if (key == "screenTint.g") { offsetScreenTint.y += value; return true; }
    if (key == "screenTint.b") { offsetScreenTint.z += value; return true; }
    if (key == "emissionStrength") { offsetEmissionStrength += value; return true; }
    return false;
}

float Part::getValue(const std::string& key) const {
    if (key == "alphaThreshold") return offsetMaskThreshold;
    if (key == "opacity") return offsetOpacity;
    if (key == "tint.r") return offsetTint.x;
    if (key == "tint.g") return offsetTint.y;
    if (key == "tint.b") return offsetTint.z;
    if (key == "screenTint.r") return offsetScreenTint.x;
    if (key == "screenTint.g") return offsetScreenTint.y;
    if (key == "screenTint.b") return offsetScreenTint.z;
    if (key == "emissionStrength") return offsetEmissionStrength;
    return Drawable::getValue(key);
}

bool Part::isMaskedBy(const std::shared_ptr<Drawable>& drawable) const {
    if (!drawable) return false;
    for (const auto& m : masks) {
        if (m.maskSrcUUID == drawable->uuid) return true;
    }
    return false;
}

std::ptrdiff_t Part::getMaskIdx(const std::shared_ptr<Drawable>& drawable) const {
    if (!drawable) return -1;
    for (std::size_t i = 0; i < masks.size(); ++i) {
        if (masks[i].maskSrcUUID == drawable->uuid) return static_cast<std::ptrdiff_t>(i);
    }
    return -1;
}

std::ptrdiff_t Part::getMaskIdx(uint32_t uid) const {
    for (std::size_t i = 0; i < masks.size(); ++i) {
        if (masks[i].maskSrcUUID == uid) return static_cast<std::ptrdiff_t>(i);
    }
    return -1;
}

void Part::runBeginTask(core::RenderContext& ctx) {
    offsetMaskThreshold = 0;
    offsetOpacity = 1;
    offsetTint = Vec3{1, 1, 1};
    offsetScreenTint = Vec3{0, 0, 0};
    offsetEmissionStrength = 1;
    syncTextureIds();
    Drawable::runBeginTask(ctx);
}

void Part::rebuffer(const MeshData& data) {
    *mesh = data;
    updateVertices();
    updateUVs();
}

void Part::setTexture(std::size_t slot, const std::shared_ptr<::nicxlive::core::Texture>& tex) {
    if (slot >= textures.size()) return;
    textures[slot] = tex;
    markTextureDirty(slot);
}

std::shared_ptr<::nicxlive::core::Texture> Part::getTexture(std::size_t slot) const {
    if (slot >= textures.size()) return nullptr;
    return textures[slot];
}

void Part::markTextureDirty(std::size_t slot) {
    if (slot < textureDirty.size()) textureDirty[slot] = true;
}

void Part::syncTextureIds() {
    auto pup = puppetRef();
    const bool inpMode = ::nicxlive::core::fmt::inIsINPMode();

    if (pup) {
        for (std::size_t i = 0; i < textureIds.size() && i < textures.size(); ++i) {
            if (textureIds[i] < 0) continue;
            const uint32_t id = static_cast<uint32_t>(textureIds[i]);
            if (id < pup->textureSlots.size() && pup->textureSlots[id]) {
                textures[i] = pup->textureSlots[id];
            } else if (!inpMode) {
                textures[i] = pup->resolveTextureSlot(id);
            }
        }
    }
    if (textureIds.size() < textures.size()) textureIds.resize(textures.size(), -1);

    if (inpMode) {
        if (pup) {
            for (std::size_t i = 0; i < textures.size(); ++i) {
                if (textureIds[i] < 0 && textures[i]) {
                    auto idx = pup->getTextureSlotIndexFor(textures[i]);
                    if (idx >= 0) {
                        textureIds[i] = static_cast<int32_t>(idx);
                    }
                }
                if (textures[i]) {
                    textureDirty[i] = false;
                }
            }
        }
        return;
    }

    for (std::size_t i = 0; i < textures.size(); ++i) {
        if (textures[i]) {
            textureDirty[i] = false;
        }
    }
}

void Part::draw() { drawSelf(false); }

void Part::runRenderTask(core::RenderContext& ctx) {
    syncTextureIds();
    enqueueRenderCommands(ctx);
}

void Part::drawOne() { drawSelf(false); }

void Part::drawOneDirect(bool forMasking) {
    if (forMasking) {
        drawSelfPacket(true);
    } else {
        // if masks exist, render them first
        auto maskBindings = dedupMasks(masks);
        if (!maskBindings.empty()) {
            core::RenderContext ctx;
            ctx.renderBackend = core::getCurrentRenderBackend().get();
            ctx.renderGraph = nullptr;
            // emulate mask rendering sequence immediately
            auto qb = std::dynamic_pointer_cast<core::render::QueueRenderBackend>(core::getCurrentRenderBackend());
            if (qb) {
                core::render::QueueCommandEmitter emitter(qb);
                emitter.beginMask(true);
                for (auto m : maskBindings) {
                    auto maskPtr = m.maskSrc;
                    if (!maskPtr) {
                        if (auto pup = puppetRef()) {
                            maskPtr = std::dynamic_pointer_cast<Drawable>(pup->findNodeById(m.maskSrcUUID));
                        }
                    }
                    if (!maskPtr) continue;
                    emitter.applyMask(maskPtr, m.mode == MaskingMode::DodgeMask);
                }
                emitter.beginMaskContent();
                emitter.drawPart(std::dynamic_pointer_cast<Part>(shared_from_this()), false);
                emitter.endMask();
            } else {
                enqueueRenderCommands(ctx);
            }
        } else {
            drawSelfPacket(false);
        }
    }
}

void Part::fillDrawPacket(const Node& header, PartDrawPacket& packet, bool isMask) const {
    packet.renderable = enabled && opacity > 0.0f && mesh->isReady();
    packet.modelMatrix = immediateModelMatrix();
    auto renderSpace = currentRenderSpace();
    packet.renderMatrix = renderSpace.matrix;
    packet.renderRotation = renderSpace.rotation;
    if (hasOffscreenModelMatrix && hasOffscreenRenderMatrix) {
        packet.renderMatrix = offscreenRenderMatrix;
        packet.renderRotation = 0.0f;
    }
    if (auto pup = puppetRef()) {
        packet.puppetMatrix = pup->transform.toMat4();
    }
    packet.ignorePuppet = ignorePuppet;
    packet.isMask = isMask;
    packet.opacity = std::clamp(offsetOpacity * opacity, 0.0f, 1.0f);
    packet.emissionStrength = emissionStrength * offsetEmissionStrength;
    packet.blendMode = blendMode;
    packet.useMultistageBlend = useMultistageBlend(blendMode);
    packet.hasEmissionOrBumpmap = (textures.size() > 2) && (textures[1] || textures[2]);
    packet.maskThreshold = std::clamp(offsetMaskThreshold + maskAlphaThreshold, 0.0f, 1.0f);
    Vec3 tintAccum = tint;
    tintAccum.x = std::clamp(tint.x * offsetTint.x, 0.0f, 1.0f);
    tintAccum.y = std::clamp(tint.y * offsetTint.y, 0.0f, 1.0f);
    tintAccum.z = std::clamp(tint.z * offsetTint.z, 0.0f, 1.0f);
    packet.clampedTint = tintAccum;
    Vec3 screenAccum = screenTint;
    screenAccum.x = std::clamp(screenTint.x + offsetScreenTint.x, 0.0f, 1.0f);
    screenAccum.y = std::clamp(screenTint.y + offsetScreenTint.y, 0.0f, 1.0f);
    screenAccum.z = std::clamp(screenTint.z + offsetScreenTint.z, 0.0f, 1.0f);
    packet.clampedScreen = screenAccum;
    packet.origin = mesh->origin;
    packet.vertexOffset = vertexOffset;
    packet.uvOffset = uvOffset;
    packet.deformOffset = deformOffset;
    packet.vertexAtlasStride = sharedVertexBufferData().size();
    packet.uvAtlasStride = sharedUvBufferData().size();
    packet.deformAtlasStride = sharedDeformBufferData().size();
    packet.vertexCount = static_cast<uint32_t>(mesh->vertices.size());
    packet.indexCount = static_cast<uint32_t>(mesh->indices.size());
    packet.indexBuffer = ibo;
    if (traceSharedEnabled()) {
        const std::size_t deformEnd = static_cast<std::size_t>(packet.deformOffset) +
                                      static_cast<std::size_t>(packet.vertexCount);
        if (deformEnd > packet.deformAtlasStride) {
            std::fprintf(stderr,
                         "[nicxlive][shared-check][OOB] part uuid=%u name=%s deformOffset=%u vertexCount=%u deformStride=%u\n",
                         uuid,
                         name.c_str(),
                         packet.deformOffset,
                         packet.vertexCount,
                         static_cast<unsigned>(packet.deformAtlasStride));
        }
        const uint32_t watchUuid = tracePartUuid();
        if (watchUuid != 0 && watchUuid == uuid && !deformation.empty()) {
            static uint64_t sTraceCounter = 0;
            ++sTraceCounter;
            if ((sTraceCounter % 30) == 0) {
                std::fprintf(stderr,
                             "[nicxlive][shared-check][part] uuid=%u name=%s deform0=(%.6f,%.6f) deformOffset=%u vertexCount=%u deformStride=%u\n",
                             uuid,
                             name.c_str(),
                             deformation.x[0],
                             deformation.y[0],
                             packet.deformOffset,
                             packet.vertexCount,
                             packet.deformAtlasStride);
            }
        }
    }
    for (std::size_t i = 0; i < textures.size() && i < 3; ++i) {
        std::shared_ptr<::nicxlive::core::Texture> tex = textures[i];
        if (tex) {
            uint32_t texId = tex->getRuntimeUUID();
            if (texId == 0) texId = tex->backendId();
            packet.textureUUIDs[i] = texId;
        } else {
            packet.textureUUIDs[i] = 0;
        }
        packet.textures[i] = tex;
    }
    packet.vertices = mesh->vertices;
    packet.uvs = mesh->uvs;
    packet.indices = mesh->indices;
    packet.deformation = deformation;
    // Keep queue backend IBO map in sync with packet handles.
    // Some load paths can leave ibo assigned while index data is not registered yet.
    if (packet.indexBuffer != 0 && !packet.indices.empty()) {
        if (auto qb = std::dynamic_pointer_cast<core::render::QueueRenderBackend>(core::getCurrentRenderBackend())) {
            if (!qb->hasDrawableIndices(packet.indexBuffer)) {
                qb->uploadDrawableIndices(packet.indexBuffer, packet.indices);
            }
        }
    }
    if ((packet.vertexOffset == 1947 ||
         packet.vertexOffset == 3267 ||
         packet.vertexOffset == 3427 ||
         packet.vertexOffset == 239 ||
         packet.vertexOffset == 0 ||
         packet.vertexOffset == 916 ||
         packet.vertexOffset == 1563 ||
         packet.vertexOffset == 1504) &&
        packet.deformation.size() > 0) {
        auto t = transform();
        auto p = parentPtr();
        const char* pType = p ? p->typeId().c_str() : "null";
        const char* pName = p ? p->name.c_str() : "null";
        std::fprintf(stderr,
                     "[nicxlive] dbg vo=%u do=%u uuid=%u parent=%u parentType=%s parentName=%s name=%s deform0=(%g,%g) localT=(%g,%g) offT=(%g,%g) worldT=(%g,%g) pre=%zu post=%zu\n",
                     static_cast<unsigned>(packet.vertexOffset),
                     static_cast<unsigned>(packet.deformOffset),
                     uuid,
                     p ? p->uuid : 0u,
                     pType,
                     pName,
                     name.c_str(),
                     packet.deformation.x[0], packet.deformation.y[0],
                     localTransform.translation.x, localTransform.translation.y,
                     offsetTransform.translation.x, offsetTransform.translation.y,
                     t.translation.x, t.translation.y,
                     preProcessFilters.size(), postProcessFilters.size());
    }
    packet.node = std::dynamic_pointer_cast<Part>(const_cast<Part*>(this)->shared_from_this());
}

Mat4 Part::immediateModelMatrix() const {
    Mat4 model = transform().toMat4();
    if (hasOffscreenModelMatrix) model = offscreenModelMatrix;
    if (overrideTransformMatrix) model = *overrideTransformMatrix;
    if (auto ot = getOneTimeTransform()) model = Mat4::multiply(*ot, model);
    return model;
}

RenderSpace Part::currentRenderSpace(bool forceIgnorePuppet) const {
    const auto& cam = ::nicxlive::core::inGetCamera();
    const auto pup = puppetRef();
    const bool usePuppet = !forceIgnorePuppet && !ignorePuppet && static_cast<bool>(pup);
    const Mat4 puppetMatrix = usePuppet ? pup->transform.toMat4() : Mat4::identity();
    const Vec2 puppetScale = usePuppet ? pup->transform.scale : Vec2{1.0f, 1.0f};
    const float puppetRot = usePuppet ? pup->transform.rotation.z : 0.0f;

    float sx = cam.scale.x * puppetScale.x;
    float sy = cam.scale.y * puppetScale.y;
    if (!std::isfinite(sx) || sx == 0.0f) sx = 1.0f;
    if (!std::isfinite(sy) || sy == 0.0f) sy = 1.0f;

    float rot = cam.rotation + puppetRot;
    if (!std::isfinite(rot)) rot = 0.0f;
    if ((sx * sy) < 0.0f) rot = -rot;

    RenderSpace space;
    space.matrix = cam.matrix() * puppetMatrix;
    space.scale = Vec2{sx, sy};
    space.rotation = rot;
    return space;
}

void Part::setOffscreenModelMatrix(const Mat4& m) { offscreenModelMatrix = m; hasOffscreenModelMatrix = true; }
void Part::setOffscreenRenderMatrix(const Mat4& m) { offscreenRenderMatrix = m; hasOffscreenRenderMatrix = true; }
void Part::clearOffscreenModelMatrix() { hasOffscreenModelMatrix = false; }
void Part::clearOffscreenRenderMatrix() { hasOffscreenRenderMatrix = false; }

bool Part::backendRenderable() const { return enabled && mesh->isReady(); }

std::size_t Part::backendMaskCount() const { return masks.size(); }

std::vector<MaskBinding> Part::backendMasks() const { return masks; }

static std::size_t maskCount(const std::vector<MaskBinding>& masks) {
    std::size_t c = 0;
    for (const auto& m : masks) {
        if (!m.maskSrc) continue;
        if (m.mode == MaskingMode::Mask || m.mode == MaskingMode::DodgeMask) ++c;
    }
    return c;
}

void Part::finalize() {
    if (auto pup = puppetRef()) {
        std::vector<MaskBinding> valid;
        for (auto& m : masks) {
            if (m.maskSrcUUID == 0) continue;
            auto node = pup->findNodeById(m.maskSrcUUID);
            m.maskSrc = std::dynamic_pointer_cast<Drawable>(node);
            // Keep unresolved bindings; runtime fallback resolves by UUID.
            valid.push_back(m);
        }
        masks = std::move(valid);
        // Resolve texture pointers from slot IDs at finalize time.
        // During node deserialize, puppet texture slots may not be ready yet.
        for (std::size_t i = 0; i < textureIds.size() && i < textures.size(); ++i) {
            if (textureIds[i] < 0) continue;
            textures[i] = pup->resolveTextureSlot(static_cast<uint32_t>(textureIds[i]));
        }
        // apply maskedBy legacy bindings
        for (auto id : maskedBy) {
            auto mnode = pup->findNodeById(id);
            auto drawable = std::dynamic_pointer_cast<Drawable>(mnode);
            if (drawable) {
                MaskBinding mb;
                mb.maskSrcUUID = id;
                mb.maskSrc = drawable;
                mb.mode = maskedByMode;
                masks.push_back(mb);
            }
        }
    }
}

void Part::setOneTimeTransform(const std::shared_ptr<Mat4>& transform) {
    Node::setOneTimeTransform(transform);
    for (auto& m : masks) {
        if (m.maskSrc) m.maskSrc->setOneTimeTransform(transform);
    }
}

void Part::normalizeUV(void* dataPtr) {
    auto* data = reinterpret_cast<MeshData*>(dataPtr);
    if (!data || textures.empty() || !textures[0]) return;
    float w = static_cast<float>(textures[0]->width());
    float h = static_cast<float>(textures[0]->height());
    if (w == 0.0f || h == 0.0f) return;
    for (auto& uv : data->uvs) {
        uv.x /= w;
        uv.y /= h;
        uv.x += 0.5f;
        uv.y += 0.5f;
    }
}

static std::vector<MaskBinding> dedupMasks(const std::vector<MaskBinding>& masks) {
    std::vector<MaskBinding> out;
    std::set<uint64_t> seen;
    for (const auto& m : masks) {
        uint64_t key = (static_cast<uint64_t>(m.maskSrcUUID) << 32) | static_cast<uint32_t>(m.mode);
        if (seen.count(key)) continue;
        seen.insert(key);
        out.push_back(m);
    }
    return out;
}

std::size_t Part::maskCount() const { return dedupMasks(masks).size(); }
std::size_t Part::dodgeCount() const {
    std::size_t c = 0;
    for (const auto& m : masks) {
        if (m.mode == MaskingMode::DodgeMask) ++c;
    }
    return c;
}

void Part::enqueueRenderCommands(core::RenderContext& ctx, const std::function<void(::nicxlive::core::RenderCommandEmitter&)>& post /*= {}*/) {
    if (!renderEnabled() || ctx.renderGraph == nullptr) return;

    auto dedupMaskBindings = [&]() {
        std::vector<MaskBinding> result;
        std::set<uint64_t> seen;
        for (const auto& m : masks) {
            if (!m.maskSrc) continue;
            uint64_t key = (static_cast<uint64_t>(m.maskSrcUUID) << 32) | static_cast<uint32_t>(m.mode);
            if (seen.count(key)) continue;
            seen.insert(key);
            result.push_back(m);
        }
        return result;
    };

    auto maskBindings = dedupMaskBindings();
    bool useStencil = false;
    for (const auto& m : maskBindings) {
        if (m.mode == MaskingMode::Mask) { useStencil = true; break; }
    }

    auto scopeHint = determineRenderScopeHint();
    if (scopeHint.skip) return;
    if (vertexOffset == 1621 || vertexOffset == 1563 || vertexOffset == 1504) {
        auto p = parentPtr();
        std::fprintf(stderr, "[nicxlive] enqueue part uuid=%u parent=%u vo=%u z=%.6f base=%.6f rel=%.6f off=%.6f scope(kind=%d token=%zu)\n",
                     uuid, p ? p->uuid : 0u, vertexOffset, zSort(), zSortBase(), relZSort(), offsetSort,
                     static_cast<int>(scopeHint.kind), scopeHint.token);
    }
    auto self = std::dynamic_pointer_cast<Part>(shared_from_this());
    auto cleanup = post;
    ctx.renderGraph->enqueueItem(zSort(), scopeHint, [self, maskBindings, useStencil, cleanup](core::RenderCommandEmitter& emitter) {
        auto part = self;
        if (!part || !part->renderEnabled()) return;
        if (!maskBindings.empty()) {
            emitter.beginMask(useStencil);
            for (auto binding : maskBindings) {
                auto maskPtr = binding.maskSrc;
                if (!maskPtr) {
                    if (auto pup = part->puppetRef()) {
                        auto node = pup->findNodeById(binding.maskSrcUUID);
                        maskPtr = std::dynamic_pointer_cast<Drawable>(node);
                    }
                }
                if (!maskPtr) continue;
                bool isDodge = binding.mode == MaskingMode::DodgeMask;
                emitter.applyMask(maskPtr, isDodge);
            }
            emitter.beginMaskContent();
        }

        emitter.drawPart(part, false);

        if (!maskBindings.empty()) {
            emitter.endMask();
        }

        if (cleanup) cleanup(emitter);
    });
}

void Part::copyFrom(const Node& src, bool clone, bool deepCopy) {
    Drawable::copyFrom(src, clone, deepCopy);
    if (auto p = dynamic_cast<const Part*>(&src)) {
        textures = p->textures;
        textureIds = p->textureIds;
        masks = p->masks;
        blendMode = p->blendMode;
        maskAlphaThreshold = p->maskAlphaThreshold;
        opacity = p->opacity;
        emissionStrength = p->emissionStrength;
        tint = p->tint;
        screenTint = p->screenTint;
        offsetMaskThreshold = 0;
        offsetOpacity = 1;
        offsetEmissionStrength = 1;
        offsetTint = Vec3{1, 1, 1};
        offsetScreenTint = Vec3{0, 0, 0};
    }
}

} // namespace nicxlive::core::nodes
