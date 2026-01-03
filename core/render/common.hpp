#pragma once

#include "../nodes/common.hpp"
#include "../nodes/drawable.hpp"
#include "../nodes/part.hpp"
#include "../nodes/projectable.hpp"

#include <memory>
#include <vector>

namespace nicxlive::core {

struct RenderGpuState {
    static RenderGpuState init() { return RenderGpuState{}; }
};

class RenderBackend {
public:
    virtual ~RenderBackend() = default;
};
class UnityRenderBackend : public RenderBackend {
public:
    // Unity DLL 側が取り出すコマンドバッファ
    virtual void submitPacket(const nodes::PartDrawPacket&) {}
    virtual void submitMask(const nodes::PartDrawPacket&) {}
    virtual void clear() {}
    virtual const std::vector<nodes::PartDrawPacket>& drawPackets() const {
        static const std::vector<nodes::PartDrawPacket> empty{};
        return empty;
    }
    virtual const std::vector<nodes::PartDrawPacket>& maskPackets() const {
        static const std::vector<nodes::PartDrawPacket> empty{};
        return empty;
    }
};

class RenderCommandEmitter {
public:
    virtual ~RenderCommandEmitter() = default;
    virtual void beginFrame(RenderBackend*, RenderGpuState&) {}
    virtual void playback(RenderBackend*) {}
    virtual void endFrame(RenderBackend*, RenderGpuState&) {}
    virtual void beginMask(bool /*useStencil*/) {}
    virtual void applyMask(const std::shared_ptr<nodes::Drawable>& /*mask*/, bool /*dodge*/) {}
    virtual void beginMaskContent() {}
    virtual void endMask() {}
    virtual void drawPartPacket(const nodes::PartDrawPacket& /*packet*/) {}
    virtual void drawPart(const std::shared_ptr<nodes::Part>& part, bool isMask) {
        if (!part) return;
        nodes::PartDrawPacket packet{};
        part->fillDrawPacket(*part, packet, isMask);
        drawPartPacket(packet);
    }
};

struct DynamicCompositePass {
    std::vector<std::shared_ptr<::nicxlive::core::Texture>> textures{};
    std::shared_ptr<::nicxlive::core::Texture> stencil{};
    nodes::Vec2 scale{1.0f, 1.0f};
    float rotationZ{0.0f};
    bool autoScaled{false};
};

// Global backend accessors (lightweight placeholder)
std::shared_ptr<RenderBackend> getCurrentRenderBackend();
void setCurrentRenderBackend(const std::shared_ptr<RenderBackend>& backend);

} // namespace nicxlive::core
