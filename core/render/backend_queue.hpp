#pragma once

#include "../render/common.hpp"
#include "../texture.hpp"
#include "../nodes/mask.hpp"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace nicxlive::core::render {

enum class RenderCommandKind {
    DrawPart,
    BeginDynamicComposite,
    EndDynamicComposite,
    BeginMask,
    ApplyMask,
    BeginMaskContent,
    EndMask,
};

struct QueuedCommand {
    RenderCommandKind kind{RenderCommandKind::DrawPart};
    nodes::PartDrawPacket partPacket{};
    RenderBackend::MaskApplyPacket maskApplyPacket{};
    DynamicCompositePass dynamicPass{};
    bool usesStencil{false};
};

struct TextureHandle {
    uint32_t id{0};
    int width{0};
    int height{0};
    int channels{4};
    bool stencil{false};
    std::vector<uint8_t> data{};
    ::nicxlive::core::Filtering filtering{::nicxlive::core::Filtering::Linear};
    ::nicxlive::core::Wrapping wrapping{::nicxlive::core::Wrapping::Clamp};
    float anisotropy{1.0f};
};

class QueueRenderBackend : public RenderBackend {
public:
    void clear() { queue.clear(); }
    void initializeDrawableResources() override {}
    void bindDrawableVao() override {}
    void createDrawableBuffers(RenderResourceHandle& outHandle) override { outHandle = nextIndexId++; }
    void uploadDrawableIndices(RenderResourceHandle id, const std::vector<uint16_t>& indices) override { indexBuffers[id] = indices; }
    void uploadSharedVertexBuffer(const ::nicxlive::core::common::Vec2Array& v) override { sharedVertices = v; }
    void uploadSharedUvBuffer(const ::nicxlive::core::common::Vec2Array& u) override { sharedUvs = u; }
    void uploadSharedDeformBuffer(const ::nicxlive::core::common::Vec2Array& d) override { sharedDeform = d; }
    void drawDrawableElements(RenderResourceHandle id, std::size_t count) override { lastDraw[id] = count; }
    void setDebugPointSize(float v) override { debugPointSize = v; }
    void setDebugLineWidth(float v) override { debugLineWidth = v; }
    void uploadDebugBuffer(const std::vector<nodes::Vec3>& positions, const std::vector<uint16_t>& indices) override {
        debugPositions = positions;
        debugIndices = indices;
    }
    void drawDebugPoints(const nodes::Vec4&, const nodes::Mat4&) override {}
    void drawDebugLines(const nodes::Vec4&, const nodes::Mat4&) override {}
    void beginMask(bool) override {}
    void applyMask(const MaskApplyPacket& packet) override { maskPackets.push_back(packet); }
    void beginMaskContent() override {}
    void endMask() override {}
    void beginDynamicComposite(const DynamicCompositePass& pass) override { dynamicStack.push_back(pass); }
    void endDynamicComposite(const DynamicCompositePass& pass) override {
        lastDynamic = pass;
        if (!dynamicStack.empty()) dynamicStack.pop_back();
    }
    void destroyDynamicComposite(const std::shared_ptr<DynamicCompositeSurface>&) override {}
    void drawPartPacket(const nodes::PartDrawPacket& packet) override {
        QueuedCommand cmd;
        cmd.kind = RenderCommandKind::DrawPart;
        cmd.partPacket = packet;
        queue.push_back(std::move(cmd));
    }

    uint32_t createTexture(const std::vector<uint8_t>& data, int width, int height, int channels, bool stencil);
    void updateTexture(uint32_t id, const std::vector<uint8_t>& data, int width, int height, int channels);
    void setTextureParams(uint32_t id, ::nicxlive::core::Filtering filtering, ::nicxlive::core::Wrapping wrapping, float anisotropy);
    void disposeTexture(uint32_t id);
    bool hasTexture(uint32_t id) const;
    const TextureHandle* getTexture(uint32_t id) const;

    // Unity DLL 側に受け渡すためのコピー出力
    std::vector<QueuedCommand> queue{};

    // Hooks to export packets for Unity
    const std::vector<QueuedCommand>& recorded() const { return queue; }

    const std::vector<QueuedCommand>& backendQueue() const { return queue; }
    std::vector<QueuedCommand>& backendQueue() { return queue; }

    const std::vector<uint16_t>* getDrawableIndices(uint32_t id) const {
        auto it = indexBuffers.find(id);
        if (it == indexBuffers.end()) return nullptr;
        return &it->second;
    }

private:
    uint32_t nextId{1};
    RenderResourceHandle nextIndexId{1};
    std::map<uint32_t, TextureHandle> textures{};
    std::map<RenderResourceHandle, std::vector<uint16_t>> indexBuffers{};
    ::nicxlive::core::common::Vec2Array sharedVertices{};
    ::nicxlive::core::common::Vec2Array sharedUvs{};
    ::nicxlive::core::common::Vec2Array sharedDeform{};
    std::vector<nodes::Vec3> debugPositions{};
    std::vector<uint16_t> debugIndices{};
    std::map<RenderResourceHandle, std::size_t> lastDraw{};
    float debugPointSize{1.0f};
    float debugLineWidth{1.0f};
    std::vector<MaskApplyPacket> maskPackets{};
    std::vector<DynamicCompositePass> dynamicStack{};
    std::optional<DynamicCompositePass> lastDynamic{};
};

class QueueCommandEmitter : public RenderCommandEmitter {
public:
    explicit QueueCommandEmitter(const std::shared_ptr<QueueRenderBackend>& backend) : backend_(backend) {}

    void beginFrame(RenderBackend* backend, RenderGpuState& state) override;
    void endFrame(RenderBackend*, RenderGpuState&) override;
    void beginMask(bool useStencil) override;
    void applyMask(const std::shared_ptr<nodes::Drawable>& mask, bool dodge) override;
    void beginMaskContent() override;
    void endMask() override;
    void drawPartPacket(const nodes::PartDrawPacket& packet) override;
    void beginDynamicComposite(const std::shared_ptr<nodes::Projectable>& composite, const DynamicCompositePass& pass) override;
    void endDynamicComposite(const std::shared_ptr<nodes::Projectable>& composite, const DynamicCompositePass& pass) override;
    void playback(RenderBackend* backend) override;
    const std::vector<QueuedCommand>& queue() const { return backendQueue(); }

    const std::vector<QueuedCommand>& recorded() const { return backendQueue(); }

private:
    const std::vector<QueuedCommand>& backendQueue() const;
    std::vector<QueuedCommand>& backendQueue();
    bool tryMakeMaskApplyPacket(const std::shared_ptr<nodes::Drawable>& drawable, bool isDodge, RenderBackend::MaskApplyPacket& packet);
    void record(RenderCommandKind kind, const std::function<void(QueuedCommand&)>& fill);

    std::shared_ptr<QueueRenderBackend> backend_{};
    RenderBackend* activeBackend_{nullptr};
    bool pendingMask{false};
    bool pendingMaskUsesStencil{false};
};

// ----- QueueRenderBackend inline methods -----
inline uint32_t QueueRenderBackend::createTexture(const std::vector<uint8_t>& data, int width, int height, int channels, bool stencil) {
    TextureHandle handle;
    handle.id = nextId++;
    handle.width = width;
    handle.height = height;
    handle.channels = channels;
    handle.stencil = stencil;
    handle.data = data;
    textures[handle.id] = handle;
    return handle.id;
}

inline void QueueRenderBackend::updateTexture(uint32_t id, const std::vector<uint8_t>& data, int width, int height, int channels) {
    auto it = textures.find(id);
    if (it == textures.end()) return;
    it->second.width = width;
    it->second.height = height;
    it->second.channels = channels;
    it->second.data = data;
}

inline void QueueRenderBackend::setTextureParams(uint32_t id, ::nicxlive::core::Filtering filtering, ::nicxlive::core::Wrapping wrapping, float anisotropy) {
    auto it = textures.find(id);
    if (it == textures.end()) return;
    it->second.filtering = filtering;
    it->second.wrapping = wrapping;
    it->second.anisotropy = anisotropy;
}

inline void QueueRenderBackend::disposeTexture(uint32_t id) { textures.erase(id); }
inline bool QueueRenderBackend::hasTexture(uint32_t id) const { return textures.find(id) != textures.end(); }
inline const TextureHandle* QueueRenderBackend::getTexture(uint32_t id) const {
    auto it = textures.find(id);
    if (it == textures.end()) return nullptr;
    return &it->second;
}

// ----- QueueCommandEmitter inline methods -----
inline const std::vector<QueuedCommand>& QueueCommandEmitter::backendQueue() const {
    static const std::vector<QueuedCommand> empty{};
    return backend_ ? backend_->backendQueue() : empty;
}
inline std::vector<QueuedCommand>& QueueCommandEmitter::backendQueue() {
    static std::vector<QueuedCommand> empty{};
    return backend_ ? backend_->backendQueue() : empty;
}

inline void QueueCommandEmitter::record(RenderCommandKind kind, const std::function<void(QueuedCommand&)>& fill) {
    auto& q = backendQueue();
    QueuedCommand cmd;
    cmd.kind = kind;
    if (fill) fill(cmd);
    q.push_back(cmd);
}

inline bool QueueCommandEmitter::tryMakeMaskApplyPacket(const std::shared_ptr<nodes::Drawable>& drawable, bool isDodge, RenderBackend::MaskApplyPacket& packet) {
    if (!drawable) return false;
    if (auto mask = std::dynamic_pointer_cast<nodes::Mask>(drawable)) {
        nodes::PartDrawPacket maskPacket{};
        mask->fillMaskDrawPacket(maskPacket);
        if (maskPacket.indexCount == 0) return false;
        packet.kind = RenderBackend::MaskDrawableKind::Mask;
        packet.maskPacket = maskPacket;
        packet.isDodge = isDodge;
        return true;
    }
    if (auto part = std::dynamic_pointer_cast<nodes::Part>(drawable)) {
        nodes::PartDrawPacket partPacket{};
        part->fillDrawPacket(*part, partPacket, true);
        if (partPacket.indexCount == 0) return false;
        packet.kind = RenderBackend::MaskDrawableKind::Part;
        packet.partPacket = partPacket;
        packet.isDodge = isDodge;
        return true;
    }
    return false;
}

inline void QueueCommandEmitter::beginFrame(RenderBackend* backend, RenderGpuState& state) {
    activeBackend_ = backend;
    state = RenderGpuState::init();
    pendingMask = false;
    pendingMaskUsesStencil = false;
    backendQueue().clear();
}

inline void QueueCommandEmitter::endFrame(RenderBackend*, RenderGpuState&) {}

inline void QueueCommandEmitter::beginMask(bool useStencil) {
    pendingMask = true;
    pendingMaskUsesStencil = useStencil;
}

inline void QueueCommandEmitter::applyMask(const std::shared_ptr<nodes::Drawable>& mask, bool dodge) {
    if (!mask) return;
    RenderBackend::MaskApplyPacket packet;
    if (!tryMakeMaskApplyPacket(mask, dodge, packet)) {
        pendingMask = false;
        return;
    }
    if (pendingMask) {
        record(RenderCommandKind::BeginMask, [&](QueuedCommand& cmd) {
            cmd.usesStencil = pendingMaskUsesStencil;
        });
        pendingMask = false;
    }
    record(RenderCommandKind::ApplyMask, [&](QueuedCommand& cmd) { cmd.maskApplyPacket = packet; });
}

inline void QueueCommandEmitter::beginMaskContent() {
    if (pendingMask) return;
    record(RenderCommandKind::BeginMaskContent, nullptr);
}

inline void QueueCommandEmitter::endMask() {
    if (pendingMask) {
        pendingMask = false;
        return;
    }
    record(RenderCommandKind::EndMask, nullptr);
}

inline void QueueCommandEmitter::drawPartPacket(const nodes::PartDrawPacket& packet) {
    record(RenderCommandKind::DrawPart, [&](QueuedCommand& cmd) { cmd.partPacket = packet; });
}

inline void QueueCommandEmitter::beginDynamicComposite(const std::shared_ptr<nodes::Projectable>&, const DynamicCompositePass& pass) {
    record(RenderCommandKind::BeginDynamicComposite, [&](QueuedCommand& cmd) { cmd.dynamicPass = pass; });
}

inline void QueueCommandEmitter::endDynamicComposite(const std::shared_ptr<nodes::Projectable>&, const DynamicCompositePass& pass) {
    record(RenderCommandKind::EndDynamicComposite, [&](QueuedCommand& cmd) { cmd.dynamicPass = pass; });
}

inline void QueueCommandEmitter::playback(RenderBackend* backend) { activeBackend_ = backend; }

} // namespace nicxlive::core::render
