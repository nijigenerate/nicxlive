#pragma once

#include "drawable.hpp"
#include "../serde.hpp"
#include "../texture.hpp"

#include <array>
#include <cmath>
#include <functional>
#include <limits>

namespace nicxlive::core {
class RenderCommandEmitter;
}

namespace nicxlive::core::nodes {

enum class MaskingMode {
    Mask,
    DodgeMask,
};

struct MaskLink {
    NodeId target{};
};

struct MaskBinding {
    NodeId maskSrcUUID{0};
    std::shared_ptr<Drawable> maskSrc{};
    MaskingMode mode{MaskingMode::Mask};
};

inline bool useMultistageBlend(BlendMode mode) {
    switch (mode) {
    case BlendMode::Screen:
    case BlendMode::Overlay:
    case BlendMode::SoftLight:
    case BlendMode::HardLight:
        return true;
    default:
        return false;
    }
}

class Part : public Drawable {
public:
    std::array<std::shared_ptr<::nicxlive::core::Texture>, 3> textures{};
    std::vector<int32_t> textureIds{};
    std::vector<MaskBinding> masks{};
    // legacy masked_by support
    std::vector<NodeId> maskedBy{};
    MaskingMode maskedByMode{MaskingMode::Mask};
    // legacy maskList support (unused in rendering, kept for compat)
    std::vector<MaskLink> maskList{};
    BlendMode blendMode{BlendMode::Normal};
    float maskAlphaThreshold{0.5f};
    float opacity{1.0f};
    float emissionStrength{1.0f};
    Vec3 tint{1.0f, 1.0f, 1.0f};
    Vec3 screenTint{0.0f, 0.0f, 0.0f};
    bool ignorePuppet{false};

    // Rendering state helpers
    std::array<bool, 3> textureDirty{{false, false, false}};

    float offsetMaskThreshold{0.0f};
    float offsetOpacity{1.0f};
    float offsetEmissionStrength{1.0f};
    Vec3 offsetTint{1.0f, 1.0f, 1.0f};
    Vec3 offsetScreenTint{0.0f, 0.0f, 0.0f};

    bool hasOffscreenModelMatrix{false};
    Mat4 offscreenModelMatrix{Mat4::identity()};

    Part() = default;
    Part(const MeshData& data, const std::array<std::shared_ptr<::nicxlive::core::Texture>, 3>& tex = {}, uint32_t uuidVal = 0);
    Part(const MeshData& data, const std::vector<std::shared_ptr<::nicxlive::core::Texture>>& tex, uint32_t uuidVal = 0);

    const std::string& typeId() const override;

    void initPartTasks();

    void updateUVs();

    virtual void drawSelf(bool isMask = false);
    void drawSelfPacket(bool isMask = false);
    static std::shared_ptr<Part> createSimple(const std::shared_ptr<::nicxlive::core::Texture>& tex, const std::string& name = "New Part");
    static std::shared_ptr<Part> createSimpleFromFile(const std::string& path, const std::string& name = "New Part");
    static std::shared_ptr<Part> createSimpleFromShallow(const ::nicxlive::core::ShallowTexture& tex, const std::string& name = "New Part");
    std::size_t maskCount() const;
    std::size_t dodgeCount() const;

    void serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const override;

    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;

    void serializePartial(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive = true) const override;

    virtual void renderMask(bool dodge = false);

    bool hasParam(const std::string& key) const override;

    float getDefaultValue(const std::string& key) const override;

    bool setValue(const std::string& key, float value) override;

    float getValue(const std::string& key) const override;

    bool isMaskedBy(const std::shared_ptr<Drawable>& drawable) const;

    std::ptrdiff_t getMaskIdx(const std::shared_ptr<Drawable>& drawable) const;

    std::ptrdiff_t getMaskIdx(uint32_t uid) const;

    void runBeginTask(core::RenderContext& ctx) override;

    void rebuffer(const MeshData& data);
    void setTexture(std::size_t slot, const std::shared_ptr<::nicxlive::core::Texture>& tex);
    std::shared_ptr<::nicxlive::core::Texture> getTexture(std::size_t slot) const;
    void markTextureDirty(std::size_t slot);
    void syncTextureIds();

    void draw() override;
    void runRenderTask(core::RenderContext&) override;
    void drawOne() override;
    void drawOneDirect(bool forMasking);
    void drawOneImmediate();
    void enqueueRenderCommands(core::RenderContext& ctx, const std::function<void(::nicxlive::core::RenderCommandEmitter&)>& post = {});
    void fillDrawPacket(const Node& header, PartDrawPacket& packet, bool isMask = false) const override;
    Mat4 immediateModelMatrix() const;
    void setOffscreenModelMatrix(const Mat4& m);
    void clearOffscreenModelMatrix();
    bool backendRenderable() const;
    std::size_t backendMaskCount() const;
    std::vector<MaskBinding> backendMasks() const;
    void finalize() override;
    void setOneTimeTransform(const std::shared_ptr<Mat4>& transform);
    void normalizeUV(void* data) override;
    void copyFrom(const Node& src, bool clone = false, bool deepCopy = true) override;
};

} // namespace nicxlive::core::nodes
