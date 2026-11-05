#pragma once

#include "node.hpp"
#include "../common/utils.hpp"
#include "../param/parameter.hpp"
#include "../param/deformation_ops.hpp"

#include <array>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <utility>
#include <optional>
#include <memory>

namespace nicxlive::core::nodes {

using ::nicxlive::core::common::Vec2Array;
using ::nicxlive::core::param::DeformationParameterBinding;

class Deformable;

struct Deformation {
    Vec2Array vertexOffsets{};

    Deformation() = default;
    Deformation(Vec2Array points) : vertexOffsets(std::move(points)) {}
    Deformation(const Deformation& other) : vertexOffsets(other.vertexOffsets.dup()) {}
    Deformation(Deformation&&) = default;
    Deformation(const param::DeformSlot& other) : vertexOffsets(other.vertexOffsets.dup()) {}
    Deformation& operator=(const Deformation& other);
    Deformation& operator=(Deformation&& other);
    operator param::DeformSlot() const;

    void update(const Vec2Array& points);

    template<class Serializer> void serialize(Serializer& writer) const {
        param::detail::serializeDeformationOffsets(writer, vertexOffsets);
    }
    template<class Serializer> void serialize(Serializer& writer) {
        param::detail::serializeDeformationOffsets(writer, vertexOffsets);
    }
    serde::ReadError deserializeFromFghj(serde::TypedJsonView data) {
        return param::detail::deserializeDeformationOffsets(data, vertexOffsets);
    }

    Deformation operator-() const;
    Deformation operator+(const Deformation& other) const;
    Deformation operator-(const Deformation& other) const;
    Deformation operator*(float s) const;

    template<deformation_source::Operand Other>
        requires (!std::same_as<std::remove_cvref_t<Other>, Deformation>)
    Deformation operator+(Other other) const {
        return deformation_source::ngBinary<deformation_source::Operation::Add>(*this, std::move(other));
    }
    template<deformation_source::Operand Other>
        requires (!std::same_as<std::remove_cvref_t<Other>, Deformation>)
    Deformation operator-(Other other) const {
        return deformation_source::ngBinary<deformation_source::Operation::Subtract>(*this, std::move(other));
    }
    template<deformation_source::Operand Other>
        requires (!std::same_as<std::remove_cvref_t<Other>, float>)
    Deformation operator*(Other other) const {
        return deformation_source::ngBinary<deformation_source::Operation::Multiply>(*this, std::move(other));
    }
    template<deformation_source::Operand Other>
    Deformation multiplyRightSource(Other other) const {
        return deformation_source::ngMultiplyRight(*this, std::move(other));
    }
    template<deformation_source::Operand Other>
        requires (!std::same_as<std::remove_cvref_t<Other>, Deformation>)
    friend Deformation operator*(Other other, const Deformation& receiver) {
        return deformation_source::ngMultiplyRight(receiver, std::move(other));
    }
};

class DeformationStack {
public:
    DeformationStack() noexcept = default;
    explicit DeformationStack(Deformable* owner);
    explicit DeformationStack(std::shared_ptr<Deformable> owner);
    DeformationStack(const DeformationStack& other);
    DeformationStack(DeformationStack&& other);
    DeformationStack& operator=(const DeformationStack& other);
    DeformationStack& operator=(DeformationStack&& other);

    void preUpdate();

    void update();

    void push(const Deformation& deform);
    void push(Deformation& deform);

private:
    friend class Deformable;
    DeformationStack(Deformable* owner, Deformable* containingOwner) noexcept
        : owner_(owner), containingOwner_(containingOwner) {}
    std::shared_ptr<Deformable> retainParent() const;
    std::shared_ptr<Node> retainContainer() const;
    Deformable* owner_{};
    // A detached Source value retains its real class handle. An embedded stack
    // borrows its containing owner to avoid an ownership cycle.
    std::shared_ptr<Deformable> retainedParent_;
    Deformable* containingOwner_{};
};

class Deformable : public Node {
private:
    // The Source pointer owns a separate buffer cell, not the containing Node.
    std::shared_ptr<Vec2Array> deformationStorageOwner_{std::make_shared<Vec2Array>()};
    // The actual nullable Source pointer is distinct from its native RAII owner.
    Vec2Array* deformationStorage_{deformationStorageOwner_.get()};
    void initDeformableTasks() {
        requirePreProcessTask();
        requirePostTask(0);
    }
public:
    Vec2Array vertices{};
    // Preserve the existing native spelling as a reference to that SAME cell.
    Vec2Array& deformation{*deformationStorageOwner_};
    DeformationStack deformStack{this, this};
    using DeformFilterArray = common::SharedArray<Node::DeformFilterHook>;
    DeformFilterArray deformPreProcessFilters{};
    DeformFilterArray deformPostProcessFilters{};

    Deformable();
    explicit Deformable(const std::shared_ptr<Node>& parent);
    Deformable(uint32_t uuid, const std::shared_ptr<Node>& parent = nullptr);
    explicit Deformable(Vec2Array verts);

    virtual Vec2Array& getVertices();
    // Const native consumers still dispatch the original non-const D property.
    const Vec2Array& getVertices() const { return const_cast<Deformable*>(this)->getVertices(); }

    virtual void updateVertices() {}
    virtual void updateBounds();

    virtual void refresh() final;
    virtual void refreshDeform() final;

    virtual void rebuffer(const Vec2Array& verts);

    virtual void pushDeformation(const Deformation& delta);

    virtual void updateDeform();

    virtual void remapDeformationBindings(const std::vector<std::size_t>& remap, const Vec2Array& replacement, std::size_t newLength);

    void preProcess() override;
    void postProcess(int id = 0) override;
    void copyFrom(const Node& src, bool clone = false, bool deepCopy = true) override;

    void runBeginTask(core::RenderContext& ctx) override;

    void runPreProcessTask(core::RenderContext& ctx) override;

    void runPostTaskImpl(std::size_t priority, core::RenderContext& ctx) override;

    void notifyDeformPushed(const Vec2Array& deform);
    void notifyDeformPushed(Deformation& deform);
    bool hasDeformPreProcessFilter(int stage, std::uintptr_t tag) const;
    bool hasDeformPostProcessFilter(int stage, std::uintptr_t tag) const;
    void upsertDeformPreProcessFilter(Node::DeformFilterHook hook, bool prepend = false);
    void upsertDeformPostProcessFilter(Node::DeformFilterHook hook, bool prepend = false);
    void removeDeformPreProcessFilter(int stage, std::uintptr_t tag);
    void removeDeformPostProcessFilter(int stage, std::uintptr_t tag);

protected:
    // Source construction runs on the complete owned most-derived object.
    struct DeferredConstruction {};
    explicit Deformable(DeferredConstruction)
        : deformationStorage_(nullptr), deformStack(nullptr, this) {}
    void constructDeformableOwned(std::shared_ptr<Node> parent) {
        constructDeformableOwned(Node::inCreateUUID(), parent);
    }
    void constructDeformableOwned(std::uint32_t id, std::shared_ptr<Node> parent) {
        Node::constructWithParent(id, parent);
        initDeformableTasks();
        deformationStorage_ = deformationStorageOwner_.get();
        deformStack = DeformationStack(this);
        getDeformation().resize(0);
    }

    virtual void onDeformPushed(const Vec2Array&) {}

public:
    // Source ref Deformation hook; append after all existing virtual slots.
    virtual void onDeformPushed(Deformation& deform) { onDeformPushed(deform.vertexOffsets); }
    // Source's mutable deformation property, appended after old virtual slots.
    virtual Vec2Array& getDeformation() {
        return *d_runtime::ngRequireObject(deformationStorage_);
    }
    virtual const Vec2Array& getDeformation() const {
        return *d_runtime::ngRequireObject(deformationStorage_);
    }
    // Pointer-alias consumers retain the real cell without retaining the Node.
    std::shared_ptr<Vec2Array> retainDeformationStorage() {
        (void)d_runtime::ngRequireObject(deformationStorage_);
        return deformationStorageOwner_;
    }
    std::shared_ptr<const Vec2Array> retainDeformationStorage() const {
        (void)d_runtime::ngRequireObject(deformationStorage_);
        return deformationStorageOwner_;
    }
protected:
    // Original value-descriptor signature; appended after existing virtual slots.
    virtual void remapDeformationBindings(common::SharedArray<std::size_t> remap,
        Vec2Array replacement, std::size_t newLength);
};

bool areDeformationNodesCompatible(const std::shared_ptr<Node>& lhs, const std::shared_ptr<Node>& rhs);

} // namespace nicxlive::core::nodes
