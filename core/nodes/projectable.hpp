#pragma once

#include "mask.hpp"

#include <cstddef>

namespace nicxlive::core {
struct DynamicCompositePass;
struct DynamicCompositeSurface;
} // namespace nicxlive::core

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
    bool hasCachedAncestorTransform{false};
    std::size_t lastAncestorTransformCheckFrame{static_cast<std::size_t>(-1)};
    bool deferredChanged{false};
    bool ancestorChangeQueued{false};
    std::size_t pendingAncestorChangeFrame{static_cast<std::size_t>(-1)};
    Vec4 maxChildrenBounds{};
    bool useMaxChildrenBounds{false};
    std::size_t maxBoundsStartFrame{0};
    std::size_t lastInitAttemptFrame{static_cast<std::size_t>(-1)};
    std::shared_ptr<Texture> stencil{};
    std::vector<std::shared_ptr<Drawable>> queuedOffscreenParts{};
    std::shared_ptr<::nicxlive::core::DynamicCompositeSurface> offscreenSurface{};

    // Core behaviour approximations
    virtual Transform fullTransform() const;
    Transform transform() override;
    Transform transform() const override;
    virtual Mat4 fullTransformMatrix() const;
    virtual Vec4 boundsFromMatrix(const std::shared_ptr<Part>& child, const Mat4& matrix) const;
    virtual bool detectAncestorTransformChange(std::size_t frameId);
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
    virtual ::nicxlive::core::DynamicCompositePass prepareDynamicCompositePass();
    virtual void dynamicRenderBegin(core::RenderContext& ctx);
    virtual void dynamicRenderEnd(core::RenderContext& ctx);
    virtual void renderNestedOffscreen(core::RenderContext& ctx);
    virtual void enqueueRenderCommands(core::RenderContext& ctx);
    void renderMask(bool dodge = false) override;
    void registerRenderTasks(core::TaskScheduler& scheduler) override;
    void runRenderBeginTask(core::RenderContext& ctx) override;
    void runRenderTask(core::RenderContext& ctx) override;
    void runRenderEndTask(core::RenderContext& ctx) override;
    void runDynamicTask(core::RenderContext& ctx) override;
    void runBeginTask(core::RenderContext& ctx) override;
    void runPostTaskImpl(std::size_t priority, core::RenderContext& ctx) override;
    void notifyChange(const std::shared_ptr<Node>& target, NotifyReason reason = NotifyReason::AttributeChanged) override;
    bool setupChild(const std::shared_ptr<Node>& child) override;
    bool releaseChild(const std::shared_ptr<Node>& child) override;
    void setupSelf() override;
    void releaseSelf() override;
    void onAncestorChanged(const std::shared_ptr<Node>& target, NotifyReason reason);
    void registerDelegatedTasks(core::TaskScheduler& scheduler);
    void delegatedRunRenderBeginTask(core::RenderContext& ctx);
    void delegatedRunRenderTask(core::RenderContext& ctx);
    void delegatedRunRenderEndTask(core::RenderContext& ctx);

    // Serialization
    void serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const override;
    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;
};

// Frame counter helpers (D: advanceProjectableFrame/currentProjectableFrame)
std::size_t advanceProjectableFrame();
std::size_t currentProjectableFrame();

} // namespace nicxlive::core::nodes
