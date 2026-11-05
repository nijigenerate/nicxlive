#pragma once

#include "projectable.hpp"
#include "../compat/source_allocator.hpp"
#include <functional>
#include <utility>

namespace nicxlive::core::nodes {

struct OffscreenState {
    bool enabled{false};
};

class Composite : public Projectable {
    template<class> friend struct d_runtime::SourceAllocator;
    static std::shared_ptr<Composite> allocateSourceCompositeReceiver();
public:
    bool propagateMeshGroup{true};
    OffscreenState offscreen{};

    Composite() = default;
    using Node::create;
    static std::shared_ptr<Composite> create(const std::shared_ptr<Node>& parent = {});
    template<class ParentProducer>
    static std::shared_ptr<Composite> createSourceDeferred(ParentProducer&& makeParent) {
        auto result = allocateSourceCompositeReceiver();
        std::shared_ptr<Node> parent = std::invoke(std::forward<ParentProducer>(makeParent));
        result->constructCompositeOwned(parent);
        return result;
    }
    explicit Composite(const MeshData& data, uint32_t uuidVal = 0);

    common::NullableString typeId() const override;

    bool propagateMeshGroupEnabled() const;
    void setPropagateMeshGroup(bool value);

    float threshold() const;
    void setThreshold(float value);

    Transform transform() override;
    Transform transform() const override;
    bool mustPropagate() const override;
    void serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const override;
    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;
    Vec4 getChildrenBounds(bool forceUpdate = true) override;
    void enableMaxChildrenBounds(const std::shared_ptr<Node>& target = nullptr) override;
    bool createSimpleMesh() override;
    bool updateDynamicRenderStateFlags() override;
    ::nicxlive::core::DynamicCompositePass prepareDynamicCompositePass() override;
    void dynamicRenderBegin(core::RenderContext& ctx) override;
    void fillDrawPacket(const Node& header, PartDrawPacket& packet, bool isMask = false) const override;
    void rebuffer(const MeshData& data) override;

    void copyFrom(const Node& src, bool clone = false, bool deepCopy = true) override;

protected:
    explicit Composite(DeferredConstruction) : Projectable(DeferredConstruction{}) {}
    void constructCompositeOwned(std::shared_ptr<Node> parent) {
        constructProjectableOwned(parent);
        autoResizedMesh = true;
    }

private:
    Vec2 deformationTranslationOffsetLocal() const;
    Vec2 compositeAutoScale() const;
    float compositeRotation() const;
    float cameraRotation() const;
    Mat4 offscreenRenderMatrix() const;
    Mat4 childOffscreenMatrix(const std::shared_ptr<Part>& child) const;
    Mat4 childCoreMatrix(const std::shared_ptr<Part>& child) const;
    Vec4 localBoundsFromMatrix(const std::shared_ptr<Part>& child, const Mat4& matrix) const;

    Vec2 prevCompositeScale{1.0f, 1.0f};
    float prevCompositeRotation{0.0f};
    float prevCameraRotation{0.0f};
    bool hasPrevCompositeScale{false};
    Vec4 maxChildrenBoundsLocal{};
    bool hasMaxChildrenBoundsLocal{false};
};

} // namespace nicxlive::core::nodes
