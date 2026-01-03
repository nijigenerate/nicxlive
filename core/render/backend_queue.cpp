#include "backend_queue.hpp"

namespace nicxlive::core::render {

uint32_t QueueRenderBackend::createTexture(const std::vector<uint8_t>& data, int width, int height, int channels, bool stencil) {
    uint32_t id = nextId++;
    TextureHandle handle;
    handle.id = id;
    handle.width = width;
    handle.height = height;
    handle.channels = channels;
    handle.stencil = stencil;
    handle.data = data;
    textures[id] = std::move(handle);
    return id;
}

void QueueRenderBackend::updateTexture(uint32_t id, const std::vector<uint8_t>& data, int width, int height, int channels) {
    auto it = textures.find(id);
    if (it == textures.end()) return;
    it->second.data = data;
    it->second.width = width;
    it->second.height = height;
    it->second.channels = channels;
}

void QueueRenderBackend::setTextureParams(uint32_t id, ::nicxlive::core::Filtering filtering, ::nicxlive::core::Wrapping wrapping, float anisotropy) {
    auto it = textures.find(id);
    if (it == textures.end()) return;
    it->second.filtering = filtering;
    it->second.wrapping = wrapping;
    it->second.anisotropy = anisotropy;
}

void QueueRenderBackend::disposeTexture(uint32_t id) {
    textures.erase(id);
}

bool QueueRenderBackend::hasTexture(uint32_t id) const {
    return textures.find(id) != textures.end();
}

const TextureHandle* QueueRenderBackend::getTexture(uint32_t id) const {
    auto it = textures.find(id);
    if (it == textures.end()) return nullptr;
    return &it->second;
}

void QueueCommandEmitter::beginFrame(RenderBackend* backend, RenderGpuState&) {
    (void)backend;
    if (auto qb = std::dynamic_pointer_cast<QueueRenderBackend>(backend_)) {
        qb->clear();
    }
}

void QueueCommandEmitter::endFrame(RenderBackend*, RenderGpuState&) {}

void QueueCommandEmitter::beginMask(bool useStencil) {
    backendQueue().push_back(Command{CommandKind::BeginMask, useStencil, {}, {}});
}

void QueueCommandEmitter::applyMask(const std::shared_ptr<nodes::Drawable>& mask, bool dodge) {
    backendQueue().push_back(Command{CommandKind::ApplyMask, dodge, mask, {}});
}

void QueueCommandEmitter::beginMaskContent() {
    backendQueue().push_back(Command{CommandKind::BeginMaskContent});
}

void QueueCommandEmitter::endMask() {
    backendQueue().push_back(Command{CommandKind::EndMask});
}

void QueueCommandEmitter::drawPartPacket(const nodes::PartDrawPacket& packet) {
    Command cmd;
    cmd.kind = CommandKind::DrawPart;
    cmd.flag = packet.isMask;
    cmd.part = packet.node;
    cmd.packet = packet;
    backendQueue().push_back(std::move(cmd));
}

void QueueCommandEmitter::playback(RenderBackend* backend) {
    (void)backend;
    // commands are retained in queue for later consumption by host (Unity DLLなど)
}

const std::vector<Command>& QueueCommandEmitter::backendQueue() const {
    static const std::vector<Command> empty{};
    if (auto qb = std::dynamic_pointer_cast<QueueRenderBackend>(backend_)) return qb->queue;
    return empty;
}

std::vector<Command>& QueueCommandEmitter::backendQueue() {
    static std::vector<Command> dummy;
    if (auto qb = std::dynamic_pointer_cast<QueueRenderBackend>(backend_)) return qb->queue;
    return dummy;
}

} // namespace nicxlive::core::render
