#pragma once

#include "../nodes/common.hpp"
#include "../nodes/drawable.hpp"
#include "../nodes/part.hpp"
#include "../nodes/projectable.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace nicxlive::core {

class Texture;

// 簡易矩形（D: rect）
struct Rect {
    float x{0};
    float y{0};
    float w{0};
    float h{0};
};

// Difference evaluation (D: diff_collect)
struct DifferenceEvaluationRegion {
    int x{0};
    int y{0};
    int width{0};
    int height{0};
};

constexpr std::size_t kDiffTileColumns = 16;
constexpr std::size_t kDiffTileCount = kDiffTileColumns * kDiffTileColumns;

struct DifferenceEvaluationResult {
    double red{0};
    double green{0};
    double blue{0};
    double alpha{0};
    uint32_t sampleCount{0};
    std::array<double, kDiffTileCount> tileTotals{};
    std::array<double, kDiffTileCount> tileCounts{};
};

using RenderResourceHandle = uint32_t;
struct DynamicCompositePass;

// Offscreen surface bundle (D: DynamicCompositeSurface)
struct DynamicCompositeSurface {
    std::array<std::shared_ptr<::nicxlive::core::Texture>, 3> textures{};
    std::size_t textureCount{0};
    std::shared_ptr<::nicxlive::core::Texture> stencil{};
    RenderResourceHandle framebuffer{0};
};

struct RenderGpuState {
    RenderResourceHandle framebuffer{0};
    RenderResourceHandle drawBuffers[8]{};
    uint8_t drawBufferCount{0};
    bool colorMask[4]{true, true, true, true};
    bool blendEnabled{false};

    static RenderGpuState init() { return RenderGpuState{}; }
};

class RenderBackend {
public:
    virtual ~RenderBackend() = default;
    enum class MaskDrawableKind { Part, Drawable, Mask };
    struct MaskApplyPacket {
        MaskDrawableKind kind{MaskDrawableKind::Part};
        nodes::PartDrawPacket partPacket{};
        nodes::PartDrawPacket maskPacket{};
        bool isDodge{false};
    };
    // Drawable geometry uploads (Unity互換の簡易インターフェース)
    virtual void initializeDrawableResources() {}
    virtual void bindDrawableVao() {}
    virtual void createDrawableBuffers(RenderResourceHandle& /*outHandle*/) {}
    virtual void uploadDrawableIndices(RenderResourceHandle /*id*/, const std::vector<uint16_t>& /*indices*/) {}
    virtual void uploadSharedVertexBuffer(const ::nicxlive::core::common::Vec2Array&) {}
    virtual void uploadSharedUvBuffer(const ::nicxlive::core::common::Vec2Array&) {}
    virtual void uploadSharedDeformBuffer(const ::nicxlive::core::common::Vec2Array&) {}
    virtual void drawDrawableElements(RenderResourceHandle /*id*/, std::size_t /*count*/) {}
    virtual void setDebugPointSize(float) {}
    virtual void setDebugLineWidth(float) {}
    virtual void uploadDebugBuffer(const std::vector<nodes::Vec3>& /*positions*/, const std::vector<uint16_t>& /*indices*/) {}
    virtual void drawDebugPoints(const nodes::Vec4& /*color*/, const nodes::Mat4& /*m*/) {}
    virtual void drawDebugLines(const nodes::Vec4& /*color*/, const nodes::Mat4& /*m*/) {}
    virtual void beginMask(bool /*useStencil*/) {}
    virtual void applyMask(const MaskApplyPacket& /*packet*/) {}
    virtual void beginMaskContent() {}
    virtual void endMask() {}
    virtual void beginDynamicComposite(const DynamicCompositePass& /*pass*/) {}
    virtual void endDynamicComposite(const DynamicCompositePass& /*pass*/) {}
    virtual void destroyDynamicComposite(const std::shared_ptr<DynamicCompositeSurface>& /*surface*/) {}
    virtual void drawPartPacket(const nodes::PartDrawPacket& /*packet*/) {}
    // Immediate テクスチャ描画
    virtual void drawTextureAtPart(const Texture& /*tex*/, const std::shared_ptr<nodes::Part>& /*part*/) {}
    virtual void drawTextureAtPosition(const Texture& /*tex*/, const nodes::Vec2& /*pos*/, float /*opacity*/,
                                       const nodes::Vec3& /*color*/, const nodes::Vec3& /*screenColor*/) {}
    virtual void drawTextureAtRect(const Texture& /*tex*/, const Rect& /*area*/, const Rect& /*uv*/,
                                   float /*opacity*/, const nodes::Vec3& /*color*/, const nodes::Vec3& /*screenColor*/,
                                   void* /*shader*/, void* /*camera*/) {}
    // Difference aggregation (default no-op)
    virtual void setDifferenceAggregationEnabled(bool) {}
    virtual bool isDifferenceAggregationEnabled() { return false; }
    virtual void setDifferenceAggregationRegion(const DifferenceEvaluationRegion&) {}
    virtual DifferenceEvaluationRegion getDifferenceAggregationRegion() { return DifferenceEvaluationRegion{}; }
    virtual bool evaluateDifferenceAggregation(RenderResourceHandle, int, int) { return false; }
    virtual bool fetchDifferenceAggregationResult(DifferenceEvaluationResult&) { return false; }
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
    virtual void beginDynamicComposite(const std::shared_ptr<nodes::Projectable>& /*composite*/, const DynamicCompositePass& /*pass*/) {}
    virtual void endDynamicComposite(const std::shared_ptr<nodes::Projectable>& /*composite*/, const DynamicCompositePass& /*pass*/) {}
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
    std::shared_ptr<DynamicCompositeSurface> surface{};
    RenderResourceHandle origBuffer{0};
    int origViewport[4]{0, 0, 0, 0};
    bool autoScaled{false};
    bool beginScene{false};
};

// Global backend accessors (lightweight placeholder)
std::shared_ptr<RenderBackend> getCurrentRenderBackend();
void setCurrentRenderBackend(const std::shared_ptr<RenderBackend>& backend);

} // namespace nicxlive::core
