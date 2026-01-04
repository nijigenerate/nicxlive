#pragma once

#include "../render/common.hpp"
#include "../texture.hpp"
#include "../nodes/mask.hpp"

#include <map>
#include <memory>
#include <optional>
#include "commands.hpp"

#include <string>
#include <vector>

namespace nicxlive::core::render {

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
    void clear();
    void initializeDrawableResources() override;
    void bindDrawableVao() override;
    void createDrawableBuffers(RenderResourceHandle& outHandle) override;
    void uploadDrawableIndices(RenderResourceHandle id, const std::vector<uint16_t>& indices) override;
    void uploadSharedVertexBuffer(const ::nicxlive::core::common::Vec2Array& v) override;
    void uploadSharedUvBuffer(const ::nicxlive::core::common::Vec2Array& u) override;
    void uploadSharedDeformBuffer(const ::nicxlive::core::common::Vec2Array& d) override;
    void drawDrawableElements(RenderResourceHandle id, std::size_t count) override;
    void setDebugPointSize(float v) override;
    void setDebugLineWidth(float v) override;
    void uploadDebugBuffer(const std::vector<nodes::Vec3>& positions, const std::vector<uint16_t>& indices) override;
    void drawDebugPoints(const nodes::Vec4&, const nodes::Mat4&) override;
    void drawDebugLines(const nodes::Vec4&, const nodes::Mat4&) override;
    void beginMask(bool useStencil) override;
    void applyMask(const MaskApplyPacket& packet) override;
    void beginMaskContent() override;
    void endMask() override;
    void beginDynamicComposite(const ::nicxlive::core::DynamicCompositePass& pass) override;
    void endDynamicComposite(const ::nicxlive::core::DynamicCompositePass& pass) override;
    void destroyDynamicComposite(const std::shared_ptr<::nicxlive::core::DynamicCompositeSurface>&) override;
    void drawPartPacket(const nodes::PartDrawPacket& packet) override;
    void drawTextureAtPart(const Texture&, const std::shared_ptr<nodes::Part>&) override;
    void drawTextureAtPosition(const Texture&, const nodes::Vec2&, float, const nodes::Vec3&, const nodes::Vec3&) override;
    void drawTextureAtRect(const Texture&, const Rect&, const Rect&, float, const nodes::Vec3&, const nodes::Vec3&, void*, void*) override;
    void setDifferenceAggregationEnabled(bool enabled) override;
    bool isDifferenceAggregationEnabled() override;
    void setDifferenceAggregationRegion(const DifferenceEvaluationRegion& region) override;
    DifferenceEvaluationRegion getDifferenceAggregationRegion() override;
    bool evaluateDifferenceAggregation(RenderResourceHandle, int, int) override;
    bool fetchDifferenceAggregationResult(DifferenceEvaluationResult& out) override;

    uint32_t createTexture(const std::vector<uint8_t>& data, int width, int height, int channels, bool stencil);
    void updateTexture(uint32_t id, const std::vector<uint8_t>& data, int width, int height, int channels);
    void setTextureParams(uint32_t id, ::nicxlive::core::Filtering filtering, ::nicxlive::core::Wrapping wrapping, float anisotropy);
    void disposeTexture(uint32_t id);
    bool hasTexture(uint32_t id) const;
    const TextureHandle* getTexture(uint32_t id) const;

    // Unity DLL 側に受け渡すためのコピー出力
    std::vector<QueuedCommand> queue{};

    // キューを別 backend に再生（D: RenderingBackend.playback 相当）
    void playback(RenderBackend* backend) const;

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
    std::vector<::nicxlive::core::DynamicCompositePass> dynamicStack{};
    std::optional<::nicxlive::core::DynamicCompositePass> lastDynamic{};
    bool differenceAggregationEnabled{false};
    DifferenceEvaluationRegion differenceRegion{};
    DifferenceEvaluationResult differenceResult{};
};

} // namespace nicxlive::core::render
