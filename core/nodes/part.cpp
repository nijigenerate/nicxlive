#include "part.hpp"
#include "dynamic_composite.hpp"

#include "../puppet.hpp"
#include "../render.hpp"
#include "../render/commands.hpp"
#include "../render/backend_queue.hpp"
#include "../runtime_state.hpp"
#include "../texture.hpp"
#include "../compat/native_access_fault.hpp"
#include "../compat/system_error.hpp"
#include "../../fmt/fmt.hpp"
#include "../debug_log.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <source_location>
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

bool tracePartDeformEnabled() {
    static int enabled = -1;
    if (enabled >= 0) return enabled != 0;
    const char* v = std::getenv("NJCX_TRACE_PART_DEFORM");
    if (!v) {
        enabled = 0;
        return false;
    }
    enabled = (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 || std::strcmp(v, "TRUE") == 0) ? 1 : 0;
    return enabled != 0;
}

bool tracePartListEnabled() {
    static int enabled = -1;
    if (enabled >= 0) return enabled != 0;
    const char* v = std::getenv("NJCX_TRACE_PART_LIST");
    if (!v) {
        enabled = 0;
        return false;
    }
    enabled = (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 || std::strcmp(v, "TRUE") == 0) ? 1 : 0;
    return enabled != 0;
}

std::optional<MaskingMode> parseMaskingModeString(const std::string& raw) {
    if (raw.empty()) return std::nullopt;
    std::string key;
    key.reserve(raw.size());
    for (char ch : raw) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }
    if (key == "mask") return MaskingMode::Mask;
    if (key == "dodgemask") return MaskingMode::DodgeMask;
    return std::nullopt;
}

}

static std::vector<MaskBinding> dedupMasks(const common::SharedArray<MaskBinding>& masks);
static void emitMasks(const std::vector<MaskBinding>& bindings, core::RenderCommandEmitter& emitter);

Part::Part() {
    initPartTasks();
    updateUVs();
}

std::shared_ptr<Part> Part::create(const std::shared_ptr<Node>& parent) {
    const auto actualParent = parent;
    ngRegisterModelResourceClassNames();
    auto result = std::shared_ptr<Part>(new Part(DeferredConstruction{}));
    result->constructPartOwned(actualParent);
    return result;
}

void Part::constructPartOwned(std::shared_ptr<Node> parent) {
    constructDrawableOwned(parent);
    initPartTasks();
    updateUVs();
}

std::shared_ptr<Part> Part::allocateSourcePartReceiver() {
    ngRegisterModelResourceClassNames();
    try {
        return std::shared_ptr<Part>(new Part(DeferredConstruction{}));
    } catch (const std::bad_alloc&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
    }
}

void Part::constructPartDataOwned(MeshData data, std::uint32_t id, std::shared_ptr<Node> parent) {
    constructDrawableDataOwned(data, id, parent);
    initPartTasks();
    updateUVs();
}

void Part::constructPartDataOwned(MeshData data,
    common::SharedArray<std::shared_ptr<::nicxlive::core::Texture>> inputTextures,
    std::uint32_t id, std::shared_ptr<Node> parent) {
    constructDrawableDataOwned(data, id, parent);
    initPartTasks();
    for (std::size_t index = 0; index < textures.size(); ++index) {
        if (index >= inputTextures.size()) break;
        textures[index] = inputTextures[index];
    }
    updateUVs();
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
    return createSimpleRetained(tex, common::NullableString(name));
}

std::shared_ptr<Part> Part::createSimpleRetained(const std::shared_ptr<::nicxlive::core::Texture>& tex,
    common::NullableString name) {
    const auto texture = d_runtime::ngRequireObject(tex);
    MeshData data;
    // Source part/package.d73-79 evaluates every virtual property in component order.
    const float x0 = static_cast<float>(-(texture->width() / 2));
    const float y0 = static_cast<float>(-(texture->height() / 2));
    const float x1 = static_cast<float>(-(texture->width() / 2));
    const float y1 = static_cast<float>(texture->height() / 2);
    const float x2 = static_cast<float>(texture->width() / 2);
    const float y2 = static_cast<float>(-(texture->height() / 2));
    const float x3 = static_cast<float>(texture->width() / 2);
    const float y3 = static_cast<float>(texture->height() / 2);
    data.vertices = {Vec2{x0, y0}, Vec2{x1, y1}, Vec2{x2, y2}, Vec2{x3, y3}};
    data.uvs = {Vec2{0, 0}, Vec2{0, 1}, Vec2{1, 0}, Vec2{1, 1}};
    data.indices = {0, 1, 2, 2, 1, 3};
    auto part = std::make_shared<Part>(data,
        std::array<std::shared_ptr<::nicxlive::core::Texture>, 3>{texture, nullptr, nullptr}, 0);
    part->setName(name);
    return part;
}

std::shared_ptr<Part> Part::createSimpleFromFile(const std::string& path, const std::string& name) {
    return createSimpleFromFileRetained(path, common::NullableString(name));
}

std::shared_ptr<Part> Part::createSimpleFromFileRetained(const std::string& path,
    common::NullableString name) {
    auto texture = std::make_shared<::nicxlive::core::Texture>(path);
    return createSimpleRetained(texture, name);
}

std::shared_ptr<Part> Part::createSimpleFromShallow(const ::nicxlive::core::ShallowTexture& shallow,
    const std::string& name) {
    return createSimpleFromShallowRetained(shallow, common::NullableString(name));
}

std::shared_ptr<Part> Part::createSimpleFromShallowRetained(const ::nicxlive::core::ShallowTexture& shallow,
    common::NullableString name) {
    auto texture = std::make_shared<::nicxlive::core::Texture>(shallow);
    return createSimpleRetained(texture, name);
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

common::NullableString Part::typeId() const {
    return common::NullableString::fromStaticLiteral("Part");
}

void Part::initPartTasks() { requireRenderTask(); }

void Part::updateUVs() {
    sharedUvResize(mesh->uvs, mesh->uvs.size());
    sharedUvMarkDirty();
}

void Part::drawSelf(bool isMask) {
    if (!renderEnabled()) return;
    auto backend = core::getCurrentRenderBackend();
    if (!backend) return;
    auto emitter = backend->createCommandEmitter();
    if (!emitter) {
        if (auto queue = std::dynamic_pointer_cast<core::render::QueueRenderBackend>(backend))
            emitter = std::make_unique<core::render::QueueCommandEmitter>(queue);
        else if (std::dynamic_pointer_cast<core::UnityRenderBackend>(backend)) {
            drawOneImmediate();
            return;
        } else {
            throw std::logic_error("Render backend does not support immediate Part drawing");
        }
    }
    // Editor previews draw individual Parts outside Puppet::draw(). A context
    // without a render graph silently discards enqueueRenderCommands(), so
    // give this immediate draw its own command-emitter scope instead.
    auto state = core::RenderGpuState::init();
    emitter->beginFrame(backend.get(), state);
    try {
        const auto bindings = isMask ? std::vector<MaskBinding>{} : dedupMasks(masks);
        if (!bindings.empty()) emitMasks(bindings, *emitter);
        emitter->drawPart(std::dynamic_pointer_cast<Part>(shared_from_this()), isMask);
        if (!bindings.empty()) emitter->endMask();
    } catch (...) {
        emitter->endFrame(backend.get(), state);
        throw;
    }
    emitter->endFrame(backend.get(), state);
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
            if (auto mask = m.maskSrc.lock()) {
                mask->fillDrawPacket(*mask, mp, true);
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
        auto maskPtr = m.maskSrc.lock();
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
    if (auto bs = data.get_optional<std::string>("blend_mode")) {
        if (auto parsed = parseBlendMode(*bs)) {
            blendMode = *parsed;
        }
    } else if (auto bm = data.get_optional<int>("blend_mode")) {
        if (*bm >= 0 && *bm <= static_cast<int>(BlendMode::SliceFromLower)) {
            blendMode = static_cast<BlendMode>(*bm);
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
    if (auto pup = puppetRef()) {
        for (std::size_t i = 0; i < textureIds.size() && i < textures.size(); ++i) {
            uint32_t id = static_cast<uint32_t>(textureIds[i]);
            auto tex = pup->resolveTextureSlot(id);
            if (tex) textures[i] = tex;
        }
    }
    masks.clear();
    if (auto mb = data.get_child_optional("masked_by")) {
        MaskingMode mode = MaskingMode::Mask;
        if (auto ms = data.get_optional<std::string>("mask_mode")) {
            if (auto parsed = parseMaskingModeString(*ms)) {
                mode = *parsed;
            }
        } else if (auto mm = data.get_optional<int>("mask_mode")) {
            int modeVal = *mm;
            if (modeVal >= 0 && modeVal <= static_cast<int>(MaskingMode::DodgeMask)) {
                mode = static_cast<MaskingMode>(modeVal);
            }
        }
        for (const auto& e : *mb) {
            MaskBinding binding;
            binding.maskSrcUUID = static_cast<uint32_t>(e.second.get_value<std::size_t>());
            binding.mode = mode;
            masks.push_back(binding);
        }
    }
    if (auto ml = data.get_child_optional("masks")) {
        for (const auto& e : *ml) {
            MaskBinding mb;
            mb.maskSrcUUID = e.second.get<uint32_t>("source", 0);
            if (auto modeText = e.second.get_optional<std::string>("mode")) {
                if (auto parsed = parseMaskingModeString(*modeText)) {
                    mb.mode = *parsed;
                }
            } else if (auto modeValOpt = e.second.get_optional<int>("mode")) {
                int modeVal = *modeValOpt;
                if (modeVal >= 0 && modeVal <= static_cast<int>(MaskingMode::DodgeMask)) {
                    mb.mode = static_cast<MaskingMode>(modeVal);
                }
            }
            masks.push_back(mb);
        }
    }
    updateUVs();
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

bool Part::hasParamSource(d_runtime::RetainedString sourceKey) {
    const auto self = shared_from_this();
    const auto key = sourceKey.view();
    if (Drawable::hasParamSource(sourceKey)) return true;
    return key == "alphaThreshold" || key == "opacity" ||
           key == "tint.r" || key == "tint.g" || key == "tint.b" ||
           key == "screenTint.r" || key == "screenTint.g" || key == "screenTint.b" ||
           key == "emissionStrength";
}

float Part::getDefaultValueSource(d_runtime::RetainedString sourceKey) {
    const auto self = shared_from_this();
    const auto key = sourceKey.view();
    float def = Drawable::getDefaultValueSource(sourceKey);
    if (!std::isnan(def)) return def;
    if (key == "alphaThreshold") return 0.0f;
    if (key == "opacity" || key == "tint.r" || key == "tint.g" || key == "tint.b") return 1.0f;
    if (key == "screenTint.r" || key == "screenTint.g" || key == "screenTint.b") return 0.0f;
    if (key == "emissionStrength") return 1.0f;
    return std::numeric_limits<float>::quiet_NaN();
}

bool Part::setValueSource(d_runtime::RetainedString sourceKey, float value) {
    const auto self = shared_from_this();
    const auto key = sourceKey.view();
    if (Drawable::setValueSource(sourceKey, value)) return true;
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

float Part::getValueSource(d_runtime::RetainedString sourceKey) {
    const auto self = shared_from_this();
    const auto key = sourceKey.view();
    if (key == "alphaThreshold") return offsetMaskThreshold;
    if (key == "opacity") return offsetOpacity;
    if (key == "tint.r") return offsetTint.x;
    if (key == "tint.g") return offsetTint.y;
    if (key == "tint.b") return offsetTint.z;
    if (key == "screenTint.r") return offsetScreenTint.x;
    if (key == "screenTint.g") return offsetScreenTint.y;
    if (key == "screenTint.b") return offsetScreenTint.z;
    if (key == "emissionStrength") return offsetEmissionStrength;
    return Drawable::getValueSource(sourceKey);
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

std::ptrdiff_t Part::getMaskIdx(std::shared_ptr<Drawable> drawable) {
    if (!drawable) return -1;
    const auto self = std::static_pointer_cast<Part>(shared_from_this());
    const auto range = masks;
    for (std::size_t index = 0; index < range.size(); ++index) {
        const auto mask = range.retainPointer(index);
        const auto source = mask->maskSrc.lock();
        const auto sourceUuid = d_runtime::ngRequireObject(source)->getUUID();
        const auto targetUuid = drawable->getUUID();
        if (sourceUuid == targetUuid) return static_cast<std::ptrdiff_t>(index);
    }
    return -1;
}

std::ptrdiff_t Part::getMaskIdx(uint32_t uuid) {
    const auto self = std::static_pointer_cast<Part>(shared_from_this());
    const auto range = masks;
    for (std::size_t index = 0; index < range.size(); ++index) {
        const auto mask = range.retainPointer(index);
        const auto source = mask->maskSrc.lock();
        if (d_runtime::ngRequireObject(source)->getUUID() == uuid)
            return static_cast<std::ptrdiff_t>(index);
    }
    return -1;
}

void Part::runBeginTask(core::RenderContext& ctx) {
    offsetMaskThreshold = 0;
    offsetOpacity = 1;
    offsetTint = Vec3{1, 1, 1};
    offsetScreenTint = Vec3{0, 0, 0};
    offsetEmissionStrength = 1;
    // Live texture references may be replaced by Projectable render targets.
    // Serialized slot indices are resolved during loading, not during rendering.
    Drawable::runBeginTask(ctx);
}

void Part::rebuffer(const MeshData& data) {
    rebufferMesh(data);
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
    while (textureIds.size() < textures.size()) textureIds.push_back(-1);

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
                    auto maskPtr = m.maskSrc.lock();
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
    packet.renderable = enabled && mesh->isReady();
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
    if (!std::isnan(offsetTint.x)) tintAccum.x = std::clamp(tint.x * offsetTint.x, 0.0f, 1.0f);
    if (!std::isnan(offsetTint.y)) tintAccum.y = std::clamp(tint.y * offsetTint.y, 0.0f, 1.0f);
    if (!std::isnan(offsetTint.z)) tintAccum.z = std::clamp(tint.z * offsetTint.z, 0.0f, 1.0f);
    packet.clampedTint = tintAccum;
    Vec3 screenAccum = screenTint;
    if (!std::isnan(offsetScreenTint.x)) screenAccum.x = std::clamp(screenTint.x + offsetScreenTint.x, 0.0f, 1.0f);
    if (!std::isnan(offsetScreenTint.y)) screenAccum.y = std::clamp(screenTint.y + offsetScreenTint.y, 0.0f, 1.0f);
    if (!std::isnan(offsetScreenTint.z)) screenAccum.z = std::clamp(screenTint.z + offsetScreenTint.z, 0.0f, 1.0f);
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
            NJCX_DBG_LOG("[nicxlive][shared-check][OOB] part uuid=%u name=%s deformOffset=%u vertexCount=%u deformStride=%u\n",
                         uuid,
                         name.c_str(),
                         packet.deformOffset,
                         packet.vertexCount,
                         static_cast<unsigned>(packet.deformAtlasStride));
        }
    }
    packet.textureCount = static_cast<uint32_t>(std::min<std::size_t>(textures.size(), 3));
    for (std::size_t i = 0; i < textures.size() && i < 3; ++i) {
        std::shared_ptr<::nicxlive::core::Texture> tex = textures[i];
        if (tex) {
            uint32_t texId = tex->getRuntimeUUID();
            if (texId == 0) texId = tex->backendId();
            packet.textureUUIDs[i] = texId;
            packet.textureBackendIds[i] = tex->backendId();
        } else {
            packet.textureUUIDs[i] = 0;
            packet.textureBackendIds[i] = 0;
        }
    }
    if (tracePartListEnabled()) {
        float maxAbs = 0.0f;
        for (std::size_t i = 0; i < deformation.size(); ++i) {
            maxAbs = std::max(maxAbs, std::max(std::fabs(deformation.xAt(i)), std::fabs(deformation.yAt(i))));
        }
        NJCX_DBG_LOG("[nicxlive][part-list] name=%s uuid=%u th0=%u vtx=%u maxAbs=%.6f\n",
                     name.c_str(),
                     uuid,
                     packet.textureUUIDs[0],
                     packet.vertexCount,
                     maxAbs);
    }
    static bool sTracePartDeform = []() {
        const char* env = std::getenv("NJCX_TRACE_PART_DEFORM");
        return env && env[0] != '\0' && env[0] != '0';
    }();
    if (sTracePartDeform && packet.vertexCount > 0) {
        float maxAbs = 0.0f;
        for (std::size_t i = 0; i < deformation.size(); ++i) {
            maxAbs = std::max(maxAbs, std::max(std::fabs(deformation.xAt(i)), std::fabs(deformation.yAt(i))));
        }
        if (maxAbs > 5.0f) {
            NJCX_DBG_LOG("[nicxlive][PartDeform] name=%s uuid=%u vtx=%u maxAbs=%.6f first=(%.6f,%.6f)\n",
                         name.c_str(),
                         uuid,
                         packet.vertexCount,
                         maxAbs,
                         deformation.size() ? deformation.xAt(0) : 0.0f,
                         deformation.size() ? deformation.yAt(0) : 0.0f);
        }
    }
    if (tracePartDeformEnabled() && packet.vertexCount > 0 && deformation.size() >= packet.vertexCount) {
        const float dx0 = deformation.xAt(0);
        const float dy0 = deformation.yAt(0);
        const float abs0 = std::max(std::fabs(dx0), std::fabs(dy0));
        const auto& atlas = sharedDeformBufferData();
        const float* atlasX = atlas.dataX();
        const float* localX = deformation.dataX();
        std::ptrdiff_t xDelta = -1;
        if (atlasX && localX) {
            xDelta = localX - atlasX;
        }
        if (abs0 > 1.0f || xDelta != static_cast<std::ptrdiff_t>(packet.deformOffset)) {
            NJCX_DBG_LOG("[nicxlive][part-deform] name=%s uuid=%u do=%u xDelta=%td vtx=%u d0=(%.6f,%.6f)\n",
                         name.c_str(), uuid, packet.deformOffset, xDelta, packet.vertexCount, dx0, dy0);
        }
    }
    // Keep queue backend IBO map in sync with packet handles.
    // Some load paths can leave ibo assigned while index data is not registered yet.
    if (packet.indexBuffer != 0 && !mesh->indices.empty()) {
        if (auto qb = std::dynamic_pointer_cast<core::render::QueueRenderBackend>(core::getCurrentRenderBackend())) {
            if (!qb->hasDrawableIndices(packet.indexBuffer)) {
                qb->uploadDrawableIndices(packet.indexBuffer, std::vector<uint16_t>(mesh->indices.begin(), mesh->indices.end()));
            }
        }
    }
    packet.node = std::static_pointer_cast<Part>(const_cast<Part*>(this)->shared_from_this());
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

std::vector<MaskBinding> Part::backendMasks() const { if (masks.empty()) return {}; return {masks.begin(), masks.end()}; }

static std::size_t maskCount(const common::SharedArray<MaskBinding>& masks) {
    std::size_t c = 0;
    for (const auto& m : masks) {
        if (m.maskSrc.expired()) continue;
        if (m.mode == MaskingMode::Mask || m.mode == MaskingMode::DodgeMask) ++c;
    }
    return c;
}

void Part::finalize() {
    Drawable::finalize();
    if (auto pup = puppetRef()) {
        std::vector<MaskBinding> valid;
        for (auto& m : masks) {
            if (m.maskSrcUUID == 0) continue;
            auto node = pup->findNodeById(m.maskSrcUUID);
            m.maskSrc = std::dynamic_pointer_cast<Drawable>(node);
            if (!m.maskSrc.expired()) {
                valid.push_back(m);
            }
        }
        masks = valid.empty() ? common::SharedArray<MaskBinding>{} : common::SharedArray<MaskBinding>(std::move(valid));
        // The typed loader already resolves real textures through its loading TLS.
        // Preserve those owners; the puppet slot table is installed after finalize.
        // Only legacy nodes without a resolved texture need the slot fallback.
        for (std::size_t i = 0; i < textureIds.size() && i < textures.size(); ++i) {
            if (textureIds[i] < 0 || textures[i]) continue;
            textures[i] = pup->resolveTextureSlot(static_cast<uint32_t>(textureIds[i]));
        }
    }
}

void Part::setOneTimeTransform(const std::shared_ptr<Mat4>& transform) {
    // Masks may refer back to an ancestor or to another masked Part. Traverse
    // each active receiver once, including when clearing the transform.
    if (settingOneTimeTransform_) return;
    settingOneTimeTransform_ = true;
    try {
        Node::setOneTimeTransform(transform);
        for (auto& m : masks) {
            if (auto mask = m.maskSrc.lock()) mask->setOneTimeTransform(transform);
        }
    } catch (...) {
        settingOneTimeTransform_ = false;
        throw;
    }
    settingOneTimeTransform_ = false;
}

void Part::normalizeUV(void* dataPtr) {
    auto* data = static_cast<MeshData*>(dataPtr);
    const auto texture = textures[0];
    if (!texture || texture->width() == 0 || texture->height() == 0) {
        Drawable::normalizeUV(dataPtr);
        return;
    }
    float w = static_cast<float>(texture->width());
    float h = static_cast<float>(texture->height());
    for (std::size_t i=0; i<data->uvs.size(); ++i) {
        auto uv=data->uvs[i];
        uv.x /= w;
        uv.y /= h;
        uv.x += 0.5f;
        uv.y += 0.5f;
    }
}

static std::vector<MaskBinding> dedupMasks(const common::SharedArray<MaskBinding>& masks) {
    std::vector<MaskBinding> out;
    std::set<uint64_t> seen;
    for (const auto& m : masks) {
        auto mask = m.maskSrc.lock();
        if (!mask) continue;
        uint64_t key = (static_cast<uint64_t>(mask->uuid) << 32) | static_cast<uint32_t>(m.mode);
        if (seen.count(key)) continue;
        seen.insert(key);
        out.push_back(m);
    }
    return out;
}

std::size_t Part::maskCount() const { return ::nicxlive::core::nodes::maskCount(masks); }
std::size_t Part::dodgeCount() const {
    std::size_t c = 0;
    for (const auto& m : masks) {
        if (m.mode == MaskingMode::DodgeMask) ++c;
    }
    return c;
}

void Part::enqueueRenderCommands(core::RenderContext& ctx, const std::function<void(::nicxlive::core::RenderCommandEmitter&)>& post /*= {}*/) {
    if (!renderEnabled() || ctx.renderGraph == nullptr) return;
    auto pup = puppetRef();

    auto dedupMaskBindings = [&]() {
        std::vector<MaskBinding> result;
        std::set<uint64_t> seen;
        for (auto m : masks) {
            auto maskPtr = m.maskSrc.lock();
            if (!maskPtr) continue;
            uint64_t key = (static_cast<uint64_t>(maskPtr->uuid) << 32) | static_cast<uint32_t>(m.mode);
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
    auto self = std::dynamic_pointer_cast<Part>(shared_from_this());
    auto cleanup = post;
    ctx.renderGraph->enqueueItem(zSort(), scopeHint, [self, maskBindings, useStencil, cleanup](core::RenderCommandEmitter& emitter) {
        auto part = self;
        if (!part || !part->renderEnabled()) return;
        if (!maskBindings.empty()) {
            emitter.beginMask(useStencil);
            for (auto binding : maskBindings) {
                auto maskPtr = binding.maskSrc.lock();
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
    if (dynamic_cast<const DynamicComposite*>(&src) && !dynamic_cast<DynamicComposite*>(this)) deepCopy = false;
    Drawable::copyFrom(src, clone, deepCopy);
    if (auto p = dynamic_cast<const Part*>(&src)) {
        offsetMaskThreshold = 0;
        offsetOpacity = 1;
        offsetEmissionStrength = 1;
        offsetTint = Vec3{0, 0, 0};
        offsetScreenTint = Vec3{0, 0, 0};
        textures = p->textures;
        textureIds = p->textureIds.duplicate();
        masks = p->masks.duplicate();
        blendMode = p->blendMode;
        maskAlphaThreshold = p->maskAlphaThreshold;
        opacity = p->opacity;
        emissionStrength = p->emissionStrength;
        tint = p->tint;
        screenTint = p->screenTint;
    }
}

void MaskBinding::serialize(::nicxlive::core::serde::TypedJsonWriter& serializer) const {
    const auto object = serializer.structBegin();
    serializer.putField("source", maskSrcUUID);
    serializer.putKey("mode");
    ::nicxlive::core::serde::serializeValue(serializer, mode);
    serializer.structEnd(object);
}

::nicxlive::core::serde::ReadError MaskBinding::deserializeFromFghj(::nicxlive::core::serde::TypedJsonView data) {
    namespace serde = ::nicxlive::core::serde;
    if (data.kind() != serde::JsonKind::Object) {
        static constexpr const char* names[] = {"null_", "false_", "number", "string", "array", "object"};
        const auto kind = data.kind() == serde::JsonKind::Boolean && data.boolean() ? "true_" :
            names[static_cast<unsigned>(data.kind())];
        return "Cann't deserialize MaskBinding. Unexpected data: " + std::string(kind);
    }
    unsigned seen = 0;
    for (const auto& [key, field] : data.members()) {
        const auto flag = key == "source" ? 1u : key == "mode" ? 2u : 0u;
        const auto member = flag == 1 ? "maskSrcUUID" : "mode";
        if (seen & flag)
            return "ASDF deserialisation: multiple keys for member '" + std::string(member) + "' in MaskBinding are not allowed.";
        seen |= flag;
        serde::ReadError error;
        if (key == "source") error = serde::deserializeValue(field, maskSrcUUID);
        else if (key == "mode") error = serde::deserializeValue(field, mode);
        if (error) return error;
    }
    if (!(seen & 1)) return "FGHJ deserialisation: non-optional member 'maskSrcUUID' in MaskBinding is missing.";
    if (!(seen & 2)) return "FGHJ deserialisation: non-optional member 'mode' in MaskBinding is missing.";
    return {};
}

} // namespace nicxlive::core::nodes

namespace nicxlive::core::serde {
namespace {
constexpr std::array<std::string_view, 19> partBlendNames{
    "Normal", "Multiply", "Screen", "Overlay", "Darken", "Lighten", "ColorDodge",
    "LinearDodge", "AddGlow", "ColorBurn", "HardLight", "SoftLight", "Difference",
    "Exclusion", "Subtract", "Inverse", "DestinationIn", "ClipToLower", "SliceFromLower"};
constexpr std::array<std::string_view, 2> partMaskNames{"Mask", "DodgeMask"};
template<class E, std::size_t N>
void serializePartEnum(TypedJsonWriter& serializer, E value, const std::array<std::string_view, N>& names) {
    const auto index = static_cast<unsigned>(value);
    if (index >= N) throw std::logic_error("Invalid part enum value");
    serializer.putValue(names[index]);
}
template<class E, std::size_t N>
ReadError deserializePartEnum(TypedJsonView data, E& value,
    const std::array<std::string_view, N>& names, std::string_view typeName) {
    std::optional<std::string> text;
    (void)deserializeValue(data, text);
    for (std::size_t i = 0; i < N; ++i) {
        if (text && *text == names[i]) { value = static_cast<E>(i); return {}; }
    }
    throw JsonConversionError("Unable to deserialize string '" + text.value_or("") + "' to " +
        std::string(typeName) + "Allowed keys:allowedKeys");
}
} // namespace
void serializeValue(TypedJsonWriter& serializer, nodes::BlendMode value) {
    serializePartEnum(serializer, value, partBlendNames);
}
void serializeValue(TypedJsonWriter& serializer, nodes::MaskingMode value) {
    serializePartEnum(serializer, value, partMaskNames);
}
ReadError deserializeValue(TypedJsonView data, nodes::BlendMode& value) {
    return deserializePartEnum(data, value, partBlendNames, "BlendMode");
}
ReadError deserializeValue(TypedJsonView data, nodes::MaskingMode& value) {
    return deserializePartEnum(data, value, partMaskNames, "MaskingMode");
}
} // namespace nicxlive::core::serde

namespace nicxlive::core::nodes {
void Part::serializeSelfImpl(serde::TypedJsonWriter& serializer, bool recursive, SerializeNodeFlags flags) const {
    Drawable::serializeSelfImpl(serializer, recursive, flags);
    if (has_flag(flags, SerializeNodeFlags::Links) && ::nicxlive::core::fmt::inIsINPMode()) {
        serializer.putKey("textures");
        const auto list = serializer.listBegin();
        for (const auto& texture : textures) {
            std::size_t index = std::numeric_limits<std::uint32_t>::max();
            if (texture) {
                const auto owner = puppetRef();
                if (!owner) throw std::runtime_error("Null Puppet while serializing Part textures");
                const auto slot = owner->getTextureSlotIndexFor(texture);
                if (slot >= 0) index = static_cast<std::size_t>(slot);
            }
            serializer.elemBegin(); serializer.putValue(index);
        }
        serializer.listEnd(list);
    }
    if (has_flag(flags, SerializeNodeFlags::State)) {
        serializer.putKey("blend_mode"); serde::serializeValue(serializer, blendMode);
        auto vector = [&](const char* key, const Vec3& value) {
            serializer.putKey(key); const auto list = serializer.listBegin();
            for (float element : {value.x, value.y, value.z}) { serializer.elemBegin(); serializer.putValue(element); }
            serializer.listEnd(list);
        };
        vector("tint", tint); vector("screenTint", screenTint);
        serializer.putField("emissionStrength", emissionStrength);
    }
    if (has_flag(flags, SerializeNodeFlags::Links) && !masks.empty()) {
        serializer.putKey("masks"); const auto list = serializer.listBegin();
        for (const auto& mask : masks) { serializer.elemBegin(); mask.serialize(serializer); }
        serializer.listEnd(list);
    }
    if (has_flag(flags, SerializeNodeFlags::State)) {
        serializer.putField("mask_threshold", maskAlphaThreshold);
        serializer.putField("opacity", opacity);
    }
}
serde::ReadError Part::deserializeFromFghj(serde::TypedJsonView data) {
    (void)Drawable::deserializeFromFghj(data);
    auto elements = [](serde::TypedJsonView value) {
        if (value.isMissing() || value.kind() != serde::JsonKind::Array)
            return std::vector<serde::TypedJsonView>{};
        return value.elements();
    };
    if (::nicxlive::core::fmt::inIsINPMode()) {
        std::size_t i = 0;
        for (const auto element : elements(data.findFirst("textures"))) {
            std::uint32_t id = 0;
            (void)serde::deserializeValue(element, id);
            if (id == std::numeric_limits<std::uint32_t>::max()) continue;
            textureIds.push_back(static_cast<std::int32_t>(id));
            textures.at(i++) = ::nicxlive::core::inGetTextureFromId(id);
        }
    } else throw std::runtime_error("Loading from texture path is deprecated.");
    (void)serde::deserializeValue(data.findFirst("opacity"), opacity);
    (void)serde::deserializeValue(data.findFirst("mask_threshold"), maskAlphaThreshold);
    auto vector = [&](serde::TypedJsonView input, Vec3& value) {
        if (input.isMissing() || input.kind() == serde::JsonKind::Null) return;
        if (input.kind() != serde::JsonKind::Array) throw serde::JsonConversionError("Expected a numeric vector array");
        const auto elements = input.elements();
        const std::array<float*, 3> coordinates{&value.x, &value.y, &value.z};
        for (std::size_t i = 0; i < elements.size() && i < coordinates.size(); ++i)
            if (serde::deserializeNumber(elements[i], *coordinates[i])) return;
    };
    if (!data.findFirst("tint").isMissing()) vector(data.findFirst("tint"), tint);
    if (!data.findFirst("screenTint").isMissing()) vector(data.findFirst("screenTint"), screenTint);
    if (!data.findFirst("emissionStrength").isMissing())
        if (auto error = serde::deserializeNumber(data.findFirst("emissionStrength"), emissionStrength)) return error;
    if (!data.findFirst("blend_mode").isMissing()) (void)serde::deserializeValue(data.findFirst("blend_mode"), blendMode);
    if (!data.findFirst("masked_by").isMissing()) {
        MaskingMode mode = MaskingMode::Mask;
        (void)serde::deserializeValue(data.findFirst("mask_mode"), mode);
        for (const auto element : elements(data.findFirst("masked_by"))) {
            std::uint32_t uuid = 0;
            if (auto error = serde::deserializeValue(element, uuid)) return error;
            masks.push_back({uuid, {}, mode});
        }
    }
    if (!data.findFirst("masks").isMissing()) (void)serde::deserializeValue(data.findFirst("masks"), masks);
    updateUVs();
    return {};
}
void Part::serializePartial(serde::TypedJsonWriter& serializer, bool recursive) const {
    Drawable::serializePartial(serializer, recursive);
    serializer.putKey("textureUUIDs"); const auto list = serializer.listBegin();
    for (const auto& texture : textures) {
        const std::uint32_t uuid = texture ? texture->getRuntimeUUID() : std::numeric_limits<std::uint32_t>::max();
        serializer.elemBegin(); serializer.putValue(static_cast<std::size_t>(uuid));
    }
    serializer.listEnd(list);
}
} // namespace nicxlive::core::nodes
