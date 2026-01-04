#include "backend_queue.hpp"

namespace nicxlive::core::render {

void QueueRenderBackend::clear() { queue.clear(); }
void QueueRenderBackend::initializeDrawableResources() {}
void QueueRenderBackend::bindDrawableVao() {}
void QueueRenderBackend::createDrawableBuffers(RenderResourceHandle& outHandle) { outHandle = nextIndexId++; }
void QueueRenderBackend::uploadDrawableIndices(RenderResourceHandle id, const std::vector<uint16_t>& indices) { indexBuffers[id] = indices; }
void QueueRenderBackend::uploadSharedVertexBuffer(const ::nicxlive::core::nodes::Vec2Array& v) { sharedVertices = v; }
void QueueRenderBackend::uploadSharedUvBuffer(const ::nicxlive::core::nodes::Vec2Array& u) { sharedUvs = u; }
void QueueRenderBackend::uploadSharedDeformBuffer(const ::nicxlive::core::nodes::Vec2Array& d) { sharedDeform = d; }
void QueueRenderBackend::drawDrawableElements(RenderResourceHandle id, std::size_t count) { lastDraw[id] = count; }
void QueueRenderBackend::setDebugPointSize(float v) { debugPointSize = v; }
void QueueRenderBackend::setDebugLineWidth(float v) { debugLineWidth = v; }
void QueueRenderBackend::uploadDebugBuffer(const std::vector<nodes::Vec3>& positions, const std::vector<uint16_t>& indices) {
    debugPositions = positions;
    debugIndices = indices;
}
void QueueRenderBackend::drawDebugPoints(const nodes::Vec4&, const nodes::Mat4&) {}
void QueueRenderBackend::drawDebugLines(const nodes::Vec4&, const nodes::Mat4&) {}
void QueueRenderBackend::beginMask(bool useStencil) {
    QueuedCommand cmd;
    cmd.kind = RenderCommandKind::BeginMask;
    cmd.usesStencil = useStencil;
    queue.push_back(cmd);
}
void QueueRenderBackend::applyMask(const MaskApplyPacket& packet) {
    QueuedCommand cmd;
    cmd.kind = RenderCommandKind::ApplyMask;
    cmd.maskApplyPacket = packet;
    queue.push_back(cmd);
}
void QueueRenderBackend::beginMaskContent() {
    QueuedCommand cmd;
    cmd.kind = RenderCommandKind::BeginMaskContent;
    queue.push_back(cmd);
}
void QueueRenderBackend::endMask() {
    QueuedCommand cmd;
    cmd.kind = RenderCommandKind::EndMask;
    queue.push_back(cmd);
}
void QueueRenderBackend::beginDynamicComposite(const ::nicxlive::core::DynamicCompositePass& pass) {
    dynamicStack.push_back(pass);
    QueuedCommand cmd;
    cmd.kind = RenderCommandKind::BeginDynamicComposite;
    cmd.dynamicPass = pass;
    queue.push_back(cmd);
}
void QueueRenderBackend::endDynamicComposite(const ::nicxlive::core::DynamicCompositePass& pass) {
    lastDynamic = pass;
    if (!dynamicStack.empty()) dynamicStack.pop_back();
    QueuedCommand cmd;
    cmd.kind = RenderCommandKind::EndDynamicComposite;
    cmd.dynamicPass = pass;
    queue.push_back(cmd);
}
void QueueRenderBackend::destroyDynamicComposite(const std::shared_ptr<::nicxlive::core::DynamicCompositeSurface>&) {}
void QueueRenderBackend::drawPartPacket(const nodes::PartDrawPacket& packet) {
    QueuedCommand cmd;
    cmd.kind = RenderCommandKind::DrawPart;
    cmd.partPacket = packet;
    queue.push_back(std::move(cmd));
}
void QueueRenderBackend::drawTextureAtPart(const Texture&, const std::shared_ptr<nodes::Part>&) {}
void QueueRenderBackend::drawTextureAtPosition(const Texture&, const nodes::Vec2&, float, const nodes::Vec3&, const nodes::Vec3&) {}
void QueueRenderBackend::drawTextureAtRect(const Texture&, const Rect&, const Rect&, float, const nodes::Vec3&, const nodes::Vec3&, void*, void*) {}
void QueueRenderBackend::setDifferenceAggregationEnabled(bool enabled) { differenceAggregationEnabled = enabled; }
bool QueueRenderBackend::isDifferenceAggregationEnabled() { return differenceAggregationEnabled; }
void QueueRenderBackend::setDifferenceAggregationRegion(const DifferenceEvaluationRegion& region) { differenceRegion = region; }
DifferenceEvaluationRegion QueueRenderBackend::getDifferenceAggregationRegion() { return differenceRegion; }
bool QueueRenderBackend::evaluateDifferenceAggregation(RenderResourceHandle, int, int) { return false; }
bool QueueRenderBackend::fetchDifferenceAggregationResult(DifferenceEvaluationResult& out) {
    out = differenceResult;
    return false;
}

uint32_t QueueRenderBackend::createTexture(const std::vector<uint8_t>& data, int width, int height, int channels, bool stencil) {
    TextureHandle handle;
    handle.id = nextId++;
    handle.width = width;
    handle.height = height;
    handle.inChannels = channels;
    handle.outChannels = channels;
    handle.stencil = stencil;
    auto expected = static_cast<std::size_t>(std::max(0, width)) * static_cast<std::size_t>(std::max(0, height)) * static_cast<std::size_t>(std::max(0, channels));
    handle.data.resize(expected);
    if (!data.empty()) {
        auto copyLen = std::min(expected, data.size());
        std::copy_n(data.begin(), copyLen, handle.data.begin());
        if (copyLen < expected) std::fill(handle.data.begin() + static_cast<std::ptrdiff_t>(copyLen), handle.data.end(), 0);
    }
    textures[handle.id] = handle;
    TextureCommand cmd;
    cmd.kind = TextureCommandKind::Create;
    cmd.id = handle.id;
    cmd.width = width;
    cmd.height = height;
    cmd.inChannels = channels;
    cmd.outChannels = channels;
    cmd.stencil = stencil;
    cmd.data = handle.data;
    resourceQueue.push_back(std::move(cmd));
    return handle.id;
}

void QueueRenderBackend::updateTexture(uint32_t id, const std::vector<uint8_t>& data, int width, int height, int channels) {
    auto it = textures.find(id);
    if (it == textures.end()) return;
    it->second.width = width;
    it->second.height = height;
    it->second.inChannels = channels;
    it->second.outChannels = channels;
    auto expected = static_cast<std::size_t>(std::max(0, width)) * static_cast<std::size_t>(std::max(0, height)) * static_cast<std::size_t>(std::max(0, channels));
    it->second.data.resize(expected);
    if (!data.empty()) {
        auto copyLen = std::min(expected, data.size());
        std::copy_n(data.begin(), copyLen, it->second.data.begin());
        if (copyLen < expected) std::fill(it->second.data.begin() + static_cast<std::ptrdiff_t>(copyLen), it->second.data.end(), 0);
    }
    TextureCommand cmd;
    cmd.kind = TextureCommandKind::Update;
    cmd.id = id;
    cmd.width = width;
    cmd.height = height;
    cmd.inChannels = channels;
    cmd.outChannels = channels;
    cmd.data = it->second.data;
    resourceQueue.push_back(std::move(cmd));
}

void QueueRenderBackend::setTextureParams(uint32_t id, ::nicxlive::core::Filtering filtering, ::nicxlive::core::Wrapping wrapping, float anisotropy) {
    auto it = textures.find(id);
    if (it == textures.end()) return;
    it->second.filtering = filtering;
    it->second.wrapping = wrapping;
    it->second.anisotropy = anisotropy;
    TextureCommand cmd;
    cmd.kind = TextureCommandKind::Params;
    cmd.id = id;
    cmd.filtering = filtering;
    cmd.wrapping = wrapping;
    cmd.anisotropy = anisotropy;
    resourceQueue.push_back(std::move(cmd));
}

void QueueRenderBackend::disposeTexture(uint32_t id) {
    textures.erase(id);
    TextureCommand cmd;
    cmd.kind = TextureCommandKind::Dispose;
    cmd.id = id;
    resourceQueue.push_back(std::move(cmd));
}
bool QueueRenderBackend::hasTexture(uint32_t id) const { return textures.find(id) != textures.end(); }
const TextureHandle* QueueRenderBackend::getTexture(uint32_t id) const {
    auto it = textures.find(id);
    if (it == textures.end()) return nullptr;
    return &it->second;
}

void QueueRenderBackend::playback(RenderBackend* backend) const {
    if (!backend) return;

    // Upload cached shared buffers/state before commands (D queue backend uploads before playback)
    if (!sharedVertices.empty()) backend->uploadSharedVertexBuffer(sharedVertices);
    if (!sharedUvs.empty()) backend->uploadSharedUvBuffer(sharedUvs);
    if (!sharedDeform.empty()) backend->uploadSharedDeformBuffer(sharedDeform);
    if (differenceAggregationEnabled) {
        backend->setDifferenceAggregationEnabled(true);
        backend->setDifferenceAggregationRegion(differenceRegion);
    } else {
        backend->setDifferenceAggregationEnabled(false);
    }

    // Replay texture/resource commands before draw commands
    for (const auto& rc : resourceQueue) {
        switch (rc.kind) {
        case TextureCommandKind::Create:
            backend->createTexture(rc.data, rc.width, rc.height, rc.inChannels, rc.stencil);
            break;
        case TextureCommandKind::Update:
            backend->updateTexture(rc.id, rc.data, rc.width, rc.height, rc.inChannels);
            break;
        case TextureCommandKind::Params:
            backend->setTextureParams(rc.id, rc.filtering, rc.wrapping, rc.anisotropy);
            break;
        case TextureCommandKind::Dispose:
            backend->disposeTexture(rc.id);
            break;
        }
    }

    for (const auto& cmd : queue) {
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
        // RenderPassKind::BeginMaskContent/EndMask are already covered by commands; mask apply is replayed below.
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

} // namespace nicxlive::core::render
