#pragma once

#include "../render/common.hpp"
#include "../texture.hpp"

#include <string>
#include <vector>
#include <memory>
#include <map>

namespace nicxlive::core::render {

enum class CommandKind {
    BeginMask,
    ApplyMask,
    BeginMaskContent,
    EndMask,
    DrawPart,
    Custom,
};

struct Command {
    CommandKind kind{CommandKind::Custom};
    bool flag{false};
    std::weak_ptr<nodes::Drawable> drawable{};
    std::weak_ptr<nodes::Part> part{};
    nodes::PartDrawPacket packet{};
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
    void uploadDrawableIndices(RenderResourceHandle id, const std::vector<uint16_t>& indices) override {
        indexBuffers[id] = indices;
    }
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
    void applyMask(const MaskApplyPacket& packet) override { maskPackets.push_back(packet); }
    uint32_t createTexture(const std::vector<uint8_t>& data, int width, int height, int channels, bool stencil);
    void updateTexture(uint32_t id, const std::vector<uint8_t>& data, int width, int height, int channels);
    void setTextureParams(uint32_t id, ::nicxlive::core::Filtering filtering, ::nicxlive::core::Wrapping wrapping, float anisotropy);
    void disposeTexture(uint32_t id);
    bool hasTexture(uint32_t id) const;
    const TextureHandle* getTexture(uint32_t id) const;
    // Unity DLL 側に受け渡すためのコピー出力
    std::vector<Command> queue{};

    // Hooks to export packets for Unity
    const std::vector<Command>& recorded() const { return queue; }

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
};

class QueueCommandEmitter : public RenderCommandEmitter {
public:
    explicit QueueCommandEmitter(const std::shared_ptr<QueueRenderBackend>& backend) : backend_(backend) {}

    void beginFrame(RenderBackend* backend, RenderGpuState&) override;
    void endFrame(RenderBackend*, RenderGpuState&) override;
    void beginMask(bool useStencil) override;
    void applyMask(const std::shared_ptr<nodes::Drawable>& mask, bool dodge) override;
    void beginMaskContent() override;
    void endMask() override;
    void drawPartPacket(const nodes::PartDrawPacket& packet) override;
    void playback(RenderBackend* backend) override;
    const std::vector<Command>& queue() const { return backendQueue(); }

    const std::vector<Command>& recorded() const { return backendQueue(); }

private:
    const std::vector<Command>& backendQueue() const;
    std::vector<Command>& backendQueue();
    std::shared_ptr<QueueRenderBackend> backend_{};
};

} // namespace nicxlive::core::render
