#pragma once

#include "driver.hpp"
#include "simple_physics_values.hpp"
#include "../serde.hpp"
#include "../param/parameter.hpp"
#include "../phys/system.hpp"
#include "../compat/source_allocator.hpp"

#include <array>
#include <limits>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <utility>

namespace nicxlive::core::nodes {

class PhysicsSystem;

class SimplePhysicsDriver : public Driver {
public:
    ~SimplePhysicsDriver() override;
    std::string name{"SimplePhysics"};
    bool active{true};
    uint32_t paramRef{std::numeric_limits<uint32_t>::max()};
    std::shared_ptr<core::param::Parameter> param_{};
    SourcePhysicsModel modelType_{PhysicsModel::Pendulum};
    // The native field spelling aliases the same Source value, without a cache.
    SourcePhysicsModel& modelType{modelType_};
    SourceParamMapMode mapMode{ParamMapMode::AngleLength};
    bool localOnly{false};
    float gravity{1.0f};
    float length{100.0f};
    float frequency{1.0f};
    float angleDamping{0.5f};
    float lengthDamping{0.5f};
    std::array<float, 2> outputScale{1.0f, 1.0f};
    std::vector<uint32_t> affectedParameters{};

    // runtime offsets
    float offsetGravity{1.0f};
    float offsetLength{0.0f};
    float offsetFrequency{1.0f};
    float offsetAngleDamping{0.5f};
    float offsetLengthDamping{0.5f};
    std::array<float, 2> offsetOutputScale{1.0f, 1.0f};
    Vec2 anchor{0.0f, 0.0f};
    Vec2 output{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    Vec2 prevAnchor{0.0f, 0.0f};
    Mat4 prevTransMat{[] {
        Mat4 value;
        for (auto& row : value.a.a)
            for (auto& element : row) element = std::numeric_limits<float>::quiet_NaN();
        return value;
    }()};
    bool prevAnchorSet{false};
    float simPhase{0.0f};
    std::weak_ptr<core::param::Parameter> paramCached{};
    // runtime state for physics integration
    float angle{0.0f};
    float dAngle{0.0f};
    float lengthVel{0.0f};
    std::shared_ptr<::nicxlive::phys::PhysicsSystem> system{};

    SimplePhysicsDriver();
    SimplePhysicsDriver(const SimplePhysicsDriver&) = delete;
    SimplePhysicsDriver& operator=(const SimplePhysicsDriver&) = delete;
    SimplePhysicsDriver(SimplePhysicsDriver&&) = delete;
    SimplePhysicsDriver& operator=(SimplePhysicsDriver&&) = delete;
    using Node::create;
    static std::shared_ptr<SimplePhysicsDriver> create(const std::shared_ptr<Node>& parent = {});
    template<class ParentProducer>
    static std::shared_ptr<SimplePhysicsDriver> createSourceDeferred(ParentProducer&& produceParent) {
        auto receiver = allocateSourcePhysicsReceiver();
        std::shared_ptr<Node> parent = std::invoke(std::forward<ParentProducer>(produceParent));
        receiver->constructSimplePhysicsOwned(std::move(parent));
        return receiver;
    }
    explicit SimplePhysicsDriver(uint32_t uuidVal, const std::shared_ptr<Node>& parent = nullptr);

    common::NullableString typeId() const override;
    void runBeginTask(core::RenderContext& ctx) override;
    void runPreProcessTask(core::RenderContext& ctx) override;
    void runPostTaskImpl(std::size_t priority, core::RenderContext& ctx) override;
    void serializeSelfImpl(::nicxlive::core::serde::TypedJsonWriter& serializer, bool recursive = true,
        SerializeNodeFlags flags = SerializeNodeFlags::All) const override;
    ::nicxlive::core::serde::ReadError deserializeFromFghj(::nicxlive::core::serde::TypedJsonView data) override;
    void serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const override;
    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;
    bool hasParam(const std::string& key) const override;
    float getDefaultValue(const std::string& key) const override;
    bool setValue(const std::string& key, float value) override;
    float getValue(const std::string& key) const override;
    void finalize() override;
    void drawDebug() override;

    void updateDriver() override;
    virtual void updateAnchors();
    void reset() override;
    std::vector<std::shared_ptr<core::param::Parameter>> getAffectedParameters() const override;
    bool affectsParameter(const std::shared_ptr<core::param::Parameter>& p) const override;

    // Source public virtual properties; keep legacy native field access separate.
    virtual std::shared_ptr<core::param::Parameter> getParam() { return param_; }
    virtual void setParam(std::shared_ptr<core::param::Parameter> value) {
        [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        [[maybe_unused]] const auto previous = param_;
        param_ = value;
        paramCached = value;
        paramRef = value ? value->getUUID() : std::numeric_limits<std::uint32_t>::max();
    }
    virtual SourcePhysicsModel getModelType() { return modelType_; }
    common::SharedArray<std::shared_ptr<core::param::Parameter>> getAffectedParametersSource() override {
        const auto parameter = param_;
        if (!parameter) return {};
        return {parameter};
    }
    virtual void setModelType(SourcePhysicsModel value) {
        modelType_ = value;
        reset();
    }

    void setParameter(const std::shared_ptr<core::param::Parameter>& p) {
        const auto value = p;
        [[maybe_unused]] const auto previous = param_;
        param_ = value;
        paramCached = value;
    }

    // helpers used by physics systems (D版互換のため公開)
    float getGravity() const;
    float getFrequency() const;
    float getAngleDamping() const;
    float getLengthDamping() const;
    Vec2 getOutputScale() const;
    void logPhysicsState(std::string_view context, std::string_view extra = "");

    bool hasParamSource(d_runtime::RetainedString sourceKey) override;
    float getDefaultValueSource(d_runtime::RetainedString sourceKey) override;
    bool setValueSource(d_runtime::RetainedString sourceKey, float value) override;
    float getValueSource(d_runtime::RetainedString sourceKey) override;

    void copyFrom(const Node& src, bool clone = false, bool deepCopy = true) override;

    virtual float getLength() const;

    // Source mutable virtual overloads follow the existing native virtual slots.
    virtual float getGravity();
    virtual float getLength();
    virtual float getFrequency();
    virtual float getAngleDamping();
    virtual float getLengthDamping();
    virtual Vec2 getOutputScale();
    virtual float getScale();

protected:
    template<class> friend struct d_runtime::SourceAllocator;
    struct DeferredConstruction {};
    explicit SimplePhysicsDriver(DeferredConstruction);
    void constructSimplePhysicsOwned(std::shared_ptr<Node> parent) {
        Driver::constructDriverOwned(Node::inCreateUUID(), parent);
        reset();
    }

private:
    static std::shared_ptr<SimplePhysicsDriver> allocateSourcePhysicsReceiver();
    void updateInputs();
    void updateOutputs();

    std::shared_ptr<core::param::Parameter> resolveParam();
};

using SimplePhysics = SimplePhysicsDriver;

} // namespace nicxlive::core::nodes

// The source module exports its concrete system classes alongside SimplePhysics.
#include "simple_physics_systems.hpp"
