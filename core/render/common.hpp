#pragma once

#include "../nodes/common.hpp"
#include "backend_handles.hpp"
#include "../common/shared_array.hpp"
#include "../compat/throwable.hpp"
#include "../math/mat4.hpp"
#include "../nodes/drawable.hpp"
#include "../nodes/part.hpp"
#include "../nodes/projectable.hpp"
#include "../texture.hpp"
#include "part_draw_packet.hpp"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <limits>
#include <stdexcept>
#include <vector>
#include <string>
#include <string_view>
#include <utility>

namespace nicxlive::core {

class Texture;
class Shader;
namespace math { class Camera; }
enum class Filtering;
enum class Wrapping;

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
    double red{std::numeric_limits<double>::quiet_NaN()};
    double green{std::numeric_limits<double>::quiet_NaN()};
    double blue{std::numeric_limits<double>::quiet_NaN()};
    double alpha{std::numeric_limits<double>::quiet_NaN()};
    uint32_t sampleCount{0};
    std::array<double, kDiffTileCount> tileTotals = [] {
        std::array<double, kDiffTileCount> values;
        values.fill(std::numeric_limits<double>::quiet_NaN());
        return values;
    }();
    std::array<double, kDiffTileCount> tileCounts = [] {
        std::array<double, kDiffTileCount> values;
        values.fill(std::numeric_limits<double>::quiet_NaN());
        return values;
    }();
    double total() const { return red + green + blue; }
};

using RenderResourceHandle = std::size_t;
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

class RenderCommandEmitter;

class RenderBackend {
public:
    virtual ~RenderBackend() = default;
    virtual std::unique_ptr<RenderCommandEmitter> createCommandEmitter();
    enum class MaskDrawableKind { Part, Drawable, Mask };
    struct MaskApplyPacket {
        MaskDrawableKind kind{MaskDrawableKind::Part};
        nodes::PartDrawPacket partPacket{};
        nodes::MaskDrawPacket maskPacket{};
        bool isDodge{false};
    };
    // Drawable geometry uploads (Unity互換の簡易インターフェース)
    virtual void initializeDrawableResources() {}
    virtual void bindDrawableVao() {}
    virtual void createDrawableBuffers(RenderResourceHandle& /*outHandle*/) {
        unsupportedShaderOperation("createDrawableBuffers");
    }
    // Release notification on the rendering thread, never from a Drawable destructor.
    virtual void disposeDrawableBuffers(RenderResourceHandle /*id*/) {}
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
    // Resource ops (D queue backend 用)
    virtual uint32_t createTexture(const std::vector<uint8_t>& /*data*/, int /*width*/, int /*height*/, int /*channels*/, bool /*stencil*/) { return 0; }
    virtual void updateTexture(uint32_t /*id*/, const std::vector<uint8_t>& /*data*/, int /*width*/, int /*height*/, int /*channels*/) {}
    virtual void setTextureParams(uint32_t /*id*/, ::nicxlive::core::Filtering /*filtering*/, ::nicxlive::core::Wrapping /*wrapping*/, float /*anisotropy*/) {}
    // This handle is the selected native backend's actual texture identifier.
    virtual std::size_t textureNativeHandle(std::uint32_t) {
        throw std::logic_error("Texture native handles are not implemented by this backend");
    }
    // D texture APIs; backends must implement real readback/upload behavior.
    virtual void readTextureData(uint32_t, int, bool, std::vector<uint8_t>&) {
        throw std::logic_error("Texture readback is not implemented by this backend");
    }
    virtual void uploadTextureData(uint32_t, int, int, int, int, bool, const std::vector<uint8_t>&) {
        throw std::logic_error("Texture upload is not implemented by this backend");
    }
    virtual void generateTextureMipmap(uint32_t) {
        throw std::logic_error("Texture mipmaps are not implemented by this backend");
    }
    virtual void disposeTexture(uint32_t /*id*/) {}
    virtual void beginDynamicComposite(DynamicCompositePass& /*pass*/) {}
    virtual void endDynamicComposite(DynamicCompositePass& /*pass*/) {}
    virtual void destroyDynamicComposite(const std::shared_ptr<DynamicCompositeSurface>& /*surface*/) {}
    virtual void drawPartPacket(const nodes::PartDrawPacket& /*packet*/) {}
    virtual void initializeRenderer() {}
    virtual void resizeViewportTargets(int /*width*/, int /*height*/) {}
    virtual void dumpViewport(std::vector<uint8_t>& dumpTo, int width, int height) {
        const auto required = static_cast<std::size_t>(std::max(0, width)) * static_cast<std::size_t>(std::max(0, height)) * 4;
        if (dumpTo.size() < required) return;
        std::fill_n(dumpTo.begin(), static_cast<std::ptrdiff_t>(required), static_cast<uint8_t>(0));
    }
    virtual RenderResourceHandle renderImageHandle() { return 0; }
    virtual RenderResourceHandle framebufferHandle() { return 0; }
    virtual RenderResourceHandle mainAlbedoHandle() { return 0; }
    virtual RenderResourceHandle mainEmissiveHandle() { return 0; }
    virtual RenderResourceHandle mainBumpHandle() { return 0; }
    virtual RenderResourceHandle compositeImageHandle() { return 0; }
    virtual RenderResourceHandle compositeFramebufferHandle() { return 0; }
    virtual RenderResourceHandle compositeEmissiveHandle() { return 0; }
    virtual RenderResourceHandle compositeBumpHandle() { return 0; }
    virtual RenderResourceHandle blendFramebufferHandle() { return 0; }
    virtual RenderResourceHandle blendAlbedoHandle() { return 0; }
    virtual RenderResourceHandle blendEmissiveHandle() { return 0; }
    virtual RenderResourceHandle blendBumpHandle() { return 0; }
    virtual void beginScene() {}
    virtual void endScene() {}
    virtual void postProcessScene() {}
    virtual void addBasicLightingPostProcess() {}
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
    // Canonical opaque Shader API. Concrete backends perform the real operations.
    virtual std::shared_ptr<RenderShaderHandle> createShader(d_runtime::RetainedString, d_runtime::RetainedString) {
        unsupportedShaderOperation("createShader");
    }
    virtual void destroyShader(std::shared_ptr<RenderShaderHandle>) { unsupportedShaderOperation("destroyShader"); }
    virtual void useShader(std::shared_ptr<RenderShaderHandle>) { unsupportedShaderOperation("useShader"); }
    virtual int getShaderUniformLocation(std::shared_ptr<RenderShaderHandle>, d_runtime::RetainedString) {
        unsupportedShaderOperation("getShaderUniformLocation");
    }
    virtual void setShaderUniform(std::shared_ptr<RenderShaderHandle>, int, bool) {
        unsupportedShaderOperation("setShaderUniform");
    }
    virtual void setShaderUniform(std::shared_ptr<RenderShaderHandle>, int, int) {
        unsupportedShaderOperation("setShaderUniform");
    }
    virtual void setShaderUniform(std::shared_ptr<RenderShaderHandle>, int, float) {
        unsupportedShaderOperation("setShaderUniform");
    }
    virtual void setShaderUniform(std::shared_ptr<RenderShaderHandle>, int, math::Vec2) {
        unsupportedShaderOperation("setShaderUniform");
    }
    virtual void setShaderUniform(std::shared_ptr<RenderShaderHandle>, int, math::Vec3) {
        unsupportedShaderOperation("setShaderUniform");
    }
    virtual void setShaderUniform(std::shared_ptr<RenderShaderHandle>, int, math::Vec4) {
        unsupportedShaderOperation("setShaderUniform");
    }
    virtual void setShaderUniform(std::shared_ptr<RenderShaderHandle>, int, math::Mat4) {
        unsupportedShaderOperation("setShaderUniform");
    }
    // Texture retains its existing canonical backend ID. This adds no second GPU resource.
    virtual void bindTextureId(std::uint32_t, std::uint32_t) {
        unsupportedShaderOperation("bindTextureHandle");
    }
private:
    [[noreturn]] static void unsupportedShaderOperation(const char* operation) {
        std::string message = "Rendering backend OpenGL does not implement ";
        message += operation;
        throw d_runtime::ThrowableException(d_runtime::RetainedString(std::move(message)));
    }
public:
    // Mutable Source Texture calls retain virtual bind dispatch. Legacy const slots stay intact.
    virtual void drawTextureAtPart(Texture& texture, const std::shared_ptr<nodes::Part>& part) {
        drawTextureAtPart(static_cast<const Texture&>(texture), part);
    }
    virtual void drawTextureAtPosition(Texture& texture, const nodes::Vec2& position, float opacity,
                                      const nodes::Vec3& color, const nodes::Vec3& screenColor) {
        drawTextureAtPosition(static_cast<const Texture&>(texture), position, opacity, color, screenColor);
    }
    virtual void drawTextureAtRect(Texture& texture, const Rect& area, const Rect& uvs, float opacity,
                                  const nodes::Vec3& color, const nodes::Vec3& screenColor,
                                  void* shader, void* camera) {
        drawTextureAtRect(static_cast<const Texture&>(texture), area, uvs, opacity, color, screenColor, shader, camera);
    }
    // Actual Source class handles and value arguments; retain locals through legacy calls.
    virtual void drawTextureAtPart(std::shared_ptr<Texture> texture, std::shared_ptr<nodes::Part> part) {
        if (!texture || !part) return;
        drawTextureAtPart(*texture, part);
    }
    virtual void drawTextureAtPosition(std::shared_ptr<Texture> texture, nodes::Vec2 position, float opacity,
                                      nodes::Vec3 color, nodes::Vec3 screenColor) {
        if (!texture) return;
        drawTextureAtPosition(*texture, position, opacity, color, screenColor);
    }
    virtual void drawTextureAtRect(std::shared_ptr<Texture> texture, Rect area, Rect uvs, float opacity,
                                  nodes::Vec3 color, nodes::Vec3 screenColor,
                                  std::shared_ptr<Shader> shader, std::shared_ptr<math::Camera> camera) {
        if (!texture) return;
        drawTextureAtRect(*texture, area, uvs, opacity, color, screenColor, shader.get(), camera.get());
    }
    // Actual Source texture handle and byte-descriptor operations on this same backend.
    virtual std::shared_ptr<RenderTextureHandle> createTextureHandle() {
        unsupportedShaderOperation("createTextureHandle");
    }
    virtual void destroyTextureHandle(std::shared_ptr<RenderTextureHandle>) {
        unsupportedShaderOperation("destroyTextureHandle");
    }
    virtual void bindTextureHandle(std::shared_ptr<RenderTextureHandle>, unsigned) {
        unsupportedShaderOperation("bindTextureHandle");
    }
    virtual void uploadTextureData(std::shared_ptr<RenderTextureHandle>, int, int, int, int, bool, SourceTextureBytes) {
        unsupportedShaderOperation("uploadTextureData");
    }
    virtual void updateTextureRegion(std::shared_ptr<RenderTextureHandle>, int, int, int, int, int, SourceTextureBytes) {
        unsupportedShaderOperation("updateTextureRegion");
    }
    virtual void generateTextureMipmap(std::shared_ptr<RenderTextureHandle>) {
        unsupportedShaderOperation("generateTextureMipmap");
    }
    virtual void applyTextureFiltering(std::shared_ptr<RenderTextureHandle>, Filtering, bool) {
        unsupportedShaderOperation("applyTextureFiltering");
    }
    virtual void applyTextureWrapping(std::shared_ptr<RenderTextureHandle>, Wrapping) {
        unsupportedShaderOperation("applyTextureWrapping");
    }
    virtual void applyTextureAnisotropy(std::shared_ptr<RenderTextureHandle>, float) {
        unsupportedShaderOperation("applyTextureAnisotropy");
    }
    virtual float maxTextureAnisotropy() { unsupportedShaderOperation("maxTextureAnisotropy"); }
    virtual void readTextureData(std::shared_ptr<RenderTextureHandle>, int, bool, SourceTextureBytes) {
        unsupportedShaderOperation("readTextureData");
    }
    virtual std::size_t textureNativeHandle(std::shared_ptr<RenderTextureHandle>) {
        unsupportedShaderOperation("textureNativeHandle");
    }
    virtual void uploadTextureData(std::uint32_t, int, int, int, int, bool, SourceTextureBytes) {
        unsupportedShaderOperation("uploadTextureData");
    }
    virtual void readTextureData(std::uint32_t, int, bool, SourceTextureBytes) {
        unsupportedShaderOperation("readTextureData");
    }
    // Source ushort[] is a retained descriptor, not a pre-call vector copy.
    virtual void uploadDrawableIndices(RenderResourceHandle, common::SharedArray<std::uint16_t>) {
        unsupportedShaderOperation("uploadDrawableIndices");
    }
    // Original mutable byte-array reference, after existing native virtual slots.
    virtual void dumpViewport(common::SharedArray<std::uint8_t>&, int, int) {
        unsupportedShaderOperation("dumpViewport");
    }
    virtual bool supportsAdvancedBlend() { unsupportedShaderOperation("supportsAdvancedBlend"); }
    virtual bool supportsAdvancedBlendCoherent() { unsupportedShaderOperation("supportsAdvancedBlendCoherent"); }
    virtual void setAdvancedBlendCoherent(bool) { unsupportedShaderOperation("setAdvancedBlendCoherent"); }
    virtual void setLegacyBlendMode(nodes::BlendMode) { unsupportedShaderOperation("setLegacyBlendMode"); }
    virtual void setAdvancedBlendEquation(nodes::BlendMode) { unsupportedShaderOperation("setAdvancedBlendEquation"); }
    virtual void issueBlendBarrier() { unsupportedShaderOperation("issueBlendBarrier"); }
    // Actual Source debug family; legacy native vector/span APIs remain separate.
    virtual void initDebugRenderer() { unsupportedShaderOperation("initDebugRenderer"); }
    // Source Queue leaves these empty; retain legacy native cache APIs unchanged.
    virtual void setDebugPointSizeSource(float value) { setDebugPointSize(value); }
    virtual void setDebugLineWidthSource(float value) { setDebugLineWidth(value); }
    virtual void uploadDebugBuffer(math::Vec3Array, common::SharedArray<std::uint16_t>) {
        unsupportedShaderOperation("uploadDebugBuffer");
    }
    virtual void setDebugExternalBuffer(RenderResourceHandle, RenderResourceHandle, int) {
        unsupportedShaderOperation("setDebugExternalBuffer");
    }
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
    virtual void endFrame(RenderBackend*, RenderGpuState&) {}
    virtual void beginMask(bool /*useStencil*/) {}
    virtual void applyMask(const std::shared_ptr<nodes::Drawable>& /*mask*/, bool /*dodge*/) {}
    virtual void beginMaskContent() {}
    virtual void endMask() {}
    virtual void drawPartPacket(const nodes::PartDrawPacket& /*packet*/) {}
    virtual void beginDynamicComposite(const std::shared_ptr<nodes::Projectable>& /*composite*/, DynamicCompositePass& /*pass*/) {}
    virtual void endDynamicComposite(const std::shared_ptr<nodes::Projectable>& /*composite*/, DynamicCompositePass& /*pass*/) {}
    // Resource ops (queue backend再生用)
    virtual uint32_t createTexture(const std::vector<uint8_t>& /*data*/, int /*width*/, int /*height*/, int /*channels*/, bool /*stencil*/) { return 0; }
    virtual void updateTexture(uint32_t /*id*/, const std::vector<uint8_t>& /*data*/, int /*width*/, int /*height*/, int /*channels*/) {}
    virtual void setTextureParams(uint32_t /*id*/, ::nicxlive::core::Filtering /*filtering*/, ::nicxlive::core::Wrapping /*wrapping*/, float /*anisotropy*/) {}
    virtual void disposeTexture(uint32_t /*id*/) {}
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
