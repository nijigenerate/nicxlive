#pragma once
#include "../common/class_reference.hpp"

#include "drawable.hpp"
#include "../serde.hpp"
#include "../texture.hpp"

#include <array>
#include <cstddef>
#include <cmath>
#include <functional>
#include <limits>
#include <utility>

namespace nicxlive::core {
class RenderCommandEmitter;
}

namespace nicxlive::core::nodes {

enum class TextureUsage : std::size_t { Albedo, Emissive, Bumpmap, COUNT };

struct RenderSpace {
    Mat4 matrix{Mat4::identity()};
    Vec2 scale{1.0f, 1.0f};
    float rotation{0.0f};
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
    common::SharedArray<int32_t> textureIds{};
    common::SharedArray<MaskBinding> masks{};
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
    bool hasOffscreenRenderMatrix{false};
    Mat4 offscreenRenderMatrix{Mat4::identity()};

    Part();
    using Node::create;
    static std::shared_ptr<Part> create(const std::shared_ptr<Node>& parent = {});
    template<class ParentProducer>
    static std::shared_ptr<Part> createSourceDeferred(ParentProducer&& makeParent) {
        auto result = allocateSourcePartReceiver();
        std::shared_ptr<Node> parent = std::invoke(std::forward<ParentProducer>(makeParent));
        result->constructPartOwned(parent);
        return result;
    }
    // Evaluate Source constructor arguments only after the exact Part owner exists.
    template<class DataProducer, class TexturesProducer, class IdProducer, class ParentProducer>
    static std::shared_ptr<Part> createDataSourceDeferred(DataProducer&& makeData,
        TexturesProducer&& makeTextures, IdProducer&& makeId, ParentProducer&& makeParent) {
        auto result = allocateSourcePartReceiver();
        MeshData data = std::invoke(std::forward<DataProducer>(makeData));
        common::SharedArray<std::shared_ptr<::nicxlive::core::Texture>> inputTextures =
            std::invoke(std::forward<TexturesProducer>(makeTextures));
        std::uint32_t id = std::invoke(std::forward<IdProducer>(makeId));
        std::shared_ptr<Node> parent = std::invoke(std::forward<ParentProducer>(makeParent));
        result->constructPartDataOwned(data, inputTextures, id, parent);
        return result;
    }
    Part(const MeshData& data, const std::array<std::shared_ptr<::nicxlive::core::Texture>, 3>& tex = {}, uint32_t uuidVal = 0);
    Part(const MeshData& data, const std::vector<std::shared_ptr<::nicxlive::core::Texture>>& tex, uint32_t uuidVal = 0);

    common::NullableString typeId() const override;

    void initPartTasks();

    void updateUVs();

    virtual void drawSelf(bool isMask = false);
    void drawSelfPacket(bool isMask = false);
    static std::shared_ptr<Part> createSimple(const std::shared_ptr<::nicxlive::core::Texture>& tex, const std::string& name = "New Part");
    static std::shared_ptr<Part> createSimpleFromFile(const std::string& path, const std::string& name = "New Part");
    static std::shared_ptr<Part> createSimpleFromShallow(const ::nicxlive::core::ShallowTexture& tex, const std::string& name = "New Part");
    // Canonical source string entries retain the actual immutable name descriptor.
    static std::shared_ptr<Part> createSimpleRetained(const std::shared_ptr<::nicxlive::core::Texture>& tex,
        common::NullableString name);
    static std::shared_ptr<Part> createSimpleFromFileRetained(const std::string& path,
        common::NullableString name);
    static std::shared_ptr<Part> createSimpleFromShallowRetained(const ::nicxlive::core::ShallowTexture& tex,
        common::NullableString name);
    std::size_t maskCount() const;
    std::size_t dodgeCount() const;

    void serializeSelfImpl(::nicxlive::core::serde::TypedJsonWriter& serializer, bool recursive = true,
        SerializeNodeFlags flags = SerializeNodeFlags::All) const override;
    ::nicxlive::core::serde::ReadError deserializeFromFghj(::nicxlive::core::serde::TypedJsonView data) override;
    void serializePartial(::nicxlive::core::serde::TypedJsonWriter& serializer, bool recursive = true) const override;
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

    void rebuffer(const MeshData& data) override;
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
    RenderSpace currentRenderSpace(bool forceIgnorePuppet = false) const;
    void fillDrawPacket(const Node& header, PartDrawPacket& packet, bool isMask = false) const override;
    Mat4 immediateModelMatrix() const;
    void setOffscreenModelMatrix(const Mat4& m);
    void setOffscreenRenderMatrix(const Mat4& m);
    void clearOffscreenModelMatrix();
    void clearOffscreenRenderMatrix();
    bool backendRenderable() const;
    std::size_t backendMaskCount() const;
    std::vector<MaskBinding> backendMasks() const;
    void finalize() override;
    void setOneTimeTransform(const std::shared_ptr<Mat4>& transform);
    void normalizeUV(void* data) override;
    void copyFrom(const Node& src, bool clone = false, bool deepCopy = true) override;

    // Source mutable virtual lookup reads actual mask receivers and virtual UUIDs.
    virtual std::ptrdiff_t getMaskIdx(std::shared_ptr<Drawable> drawable);
    virtual std::ptrdiff_t getMaskIdx(uint32_t uuid);

    bool hasParamSource(d_runtime::RetainedString sourceKey) override;
    float getDefaultValueSource(d_runtime::RetainedString sourceKey) override;
    bool setValueSource(d_runtime::RetainedString sourceKey, float value) override;
    float getValueSource(d_runtime::RetainedString sourceKey) override;

private:
    static std::shared_ptr<Part> allocateSourcePartReceiver();
protected:
    explicit Part(DeferredConstruction) : Drawable(DeferredConstruction{}) {}
    void constructPartOwned(std::shared_ptr<Node> parent);
    void constructPartDataOwned(MeshData data, std::uint32_t id, std::shared_ptr<Node> parent);
    void constructPartDataOwned(MeshData data,
        common::SharedArray<std::shared_ptr<::nicxlive::core::Texture>> inputTextures,
        std::uint32_t id, std::shared_ptr<Node> parent);
};

} // namespace nicxlive::core::nodes

