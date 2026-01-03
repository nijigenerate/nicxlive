#pragma once

#include "mask.hpp"

#include <cstddef>

namespace nicxlive::core::nodes {

class Projectable : public Part {
public:
    bool initialized{false};
    bool forceResize{false};
    bool dynamicScopeActive{false};
    std::size_t dynamicScopeToken{static_cast<std::size_t>(-1)};
    bool reuseCachedTextureThisFrame{false};
    bool hasValidOffscreenContent{false};
    bool loggedFirstRenderAttempt{false};

    bool textureInvalidated{false};
    bool shouldUpdateVertices{false};
    bool boundsDirty{true};

    std::vector<std::shared_ptr<Part>> subParts{};
    std::vector<std::shared_ptr<Mask>> maskParts{};

    Projectable() = default;
    explicit Projectable(bool delegatedMode);

    std::size_t dynamicScopeTokenValue() const { return dynamicScopeToken; }

    virtual void selfSort();

    void setIgnorePuppetRecurse(const std::shared_ptr<Node>& node, bool ignore);
    void setIgnorePuppet(bool ignore);

    void scanPartsRecurse(const std::shared_ptr<Node>& node);

    void drawSelf(bool isMask = false) override;

    // Projectable-specific state
    bool autoResizedMesh{true};
    Vec2 textureOffset{};
    uint32_t texWidth{0};
    uint32_t texHeight{0};
    Vec2 autoResizedSize{};
    int deferred{0};
    Vec3 prevTranslation{};
    Vec3 prevRotation{};
    Vec2 prevScale{};
    bool deferredChanged{false};
    bool ancestorChangeQueued{false};
    Vec4 maxChildrenBounds{};
    bool useMaxChildrenBounds{false};
    std::size_t maxBoundsStartFrame{0};
    std::size_t lastInitAttemptFrame{static_cast<std::size_t>(-1)};
    std::shared_ptr<Texture> stencil{};
    std::vector<std::shared_ptr<Drawable>> queuedOffscreenParts{};

    // Core behaviour approximations
    virtual Transform fullTransform() const;
    virtual Mat4 fullTransformMatrix() const;
    virtual Vec4 boundsFromMatrix(const std::shared_ptr<Part>& child, const Mat4& matrix) const;
    virtual Vec4 getChildrenBounds(bool forceUpdate = true);
    virtual Vec4 mergeBounds(const std::vector<Vec4>& bounds, const Vec4& origin = Vec4{});
    virtual Vec2 deformationTranslationOffset() const;
    virtual void enableMaxChildrenBounds(const std::shared_ptr<Node>& target = nullptr);
    virtual void invalidateChildrenBounds();
    virtual bool createSimpleMesh();
    virtual bool updateAutoResizedMeshOnce(bool& ran);
    virtual bool updatePartMeshOnce(bool& ran);
    virtual bool autoResizeMeshOnce(bool& ran);
    virtual bool initTarget();
    virtual bool updateDynamicRenderStateFlags();
    virtual void dynamicRenderBegin(core::RenderContext& ctx);
    virtual void dynamicRenderEnd(core::RenderContext& ctx);
    virtual void enqueueRenderCommands(core::RenderContext& ctx);
    void runRenderTask(core::RenderContext& ctx) override;
    void runBeginTask(core::RenderContext& ctx) override;
    void runPostTaskImpl(std::size_t priority, core::RenderContext& ctx) override;
    void notifyChange(const std::shared_ptr<Node>& target, NotifyReason reason = NotifyReason::AttributeChanged) override;

    // Serialization
    void serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const override;
    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;
};

} // namespace nicxlive::core::nodes
