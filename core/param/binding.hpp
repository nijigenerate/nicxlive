#pragma once

#include "../resource.hpp"
#include "../nodes/common.hpp"
#include "../nodes/node.hpp"
#include "../common/utils.hpp"
#include "../serde.hpp"
#include "../serde_reader.hpp"
#include "../serde_json.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nicxlive::core::param {

using nodes::Vec2;
using nodes::Vec3;
using nodes::Vec4;
using nodes::Node;
using common::InterpolateMode;
namespace core { class Puppet; }

class Parameter;

enum class BindTargetIdType { StringId, IntId };
// Allocate the actual concrete class before executing Source constructor callbacks.
struct OwnedBindingConstruction {};

struct Vec2u {
    std::uint32_t x{};
    std::uint32_t y{};
    constexpr Vec2u() = default;
    constexpr Vec2u(std::size_t xValue, std::size_t yValue = 0) noexcept
        : x(static_cast<std::uint32_t>(xValue)), y(static_cast<std::uint32_t>(yValue)) {}
};

// One retained Resource identity. Legacy Node/Parameter views never own a
// second target and cannot disagree after retargeting or a descriptor copy.
class BindingResourceTarget {
public:
    BindingResourceTarget() = default;
    BindingResourceTarget(std::nullptr_t) noexcept {}
    BindingResourceTarget(std::shared_ptr<::nicxlive::core::Resource> value) : value_(std::move(value)) {}
    BindingResourceTarget& operator=(std::shared_ptr<::nicxlive::core::Resource> value) {
        value_ = std::move(value); return *this;
    }
    BindingResourceTarget& operator=(std::nullptr_t) noexcept { value_.reset(); return *this; }
    std::shared_ptr<::nicxlive::core::Resource> resource() const { return value_; }
    std::shared_ptr<Node> lock() const { return std::dynamic_pointer_cast<Node>(value_); }
    void reset() noexcept { value_.reset(); }
private:
    std::shared_ptr<::nicxlive::core::Resource> value_;
};

struct BindTarget {
    BindingResourceTarget target{};
    common::NullableString name{};
    uint32_t uuid{0};

    class ParameterView {
    public:
        explicit ParameterView(BindingResourceTarget& target) noexcept : target_(&target) {}
        // A detached compatibility-view copy retains its own descriptor rather
        // than pointing back into a temporary BindTarget.
        ParameterView(const ParameterView& other)
            : detached_(other.target_->resource()), target_(&detached_) {}
        std::shared_ptr<Parameter> lock() const;
        ParameterView& operator=(const std::shared_ptr<Parameter>& value);
        ParameterView& operator=(const std::weak_ptr<Parameter>& value);
        ParameterView& operator=(const ParameterView& other);
        // Resetting the old Parameter view must not clear a current Node target.
        void reset();
    private:
        BindingResourceTarget detached_{};
        BindingResourceTarget* target_;
    } parameterTarget{target};

    BindTarget() = default;
    BindTarget(const BindTarget& other)
        : target(other.target), name(other.name), uuid(other.uuid), parameterTarget(target) {}
    BindTarget(BindTarget&& other) noexcept : BindTarget(static_cast<const BindTarget&>(other)) {}
    BindTarget& operator=(const BindTarget& other) {
        if (this != &other) { target = other.target; name = other.name; uuid = other.uuid; }
        return *this;
    }
    BindTarget& operator=(BindTarget&& other) noexcept { return *this = static_cast<const BindTarget&>(other); }
    std::shared_ptr<::nicxlive::core::Resource> getResource() const { return target.resource(); }
};

class ParameterBinding : public std::enable_shared_from_this<ParameterBinding> {
public:
    ParameterBinding() = default;
    ParameterBinding(const ParameterBinding&) = delete;
    ParameterBinding& operator=(const ParameterBinding&) = delete;
    ParameterBinding(ParameterBinding&&) = delete;
    ParameterBinding& operator=(ParameterBinding&&) = delete;
    virtual ~ParameterBinding() = default;
    virtual d_runtime::RetainedString toString() const {
        ngRegisterModelResourceClassNames();
        return d_runtime::ngObjectClassName(typeid(*this));
    }

    virtual void reconstruct(const std::shared_ptr<::nicxlive::core::Puppet>& /*puppet*/) {}
    virtual void finalize(const std::shared_ptr<::nicxlive::core::Puppet>& /*puppet*/) {}

    virtual void apply(const Vec2u& leftKeypoint, const Vec2& offset) = 0;
    virtual void clear() = 0;
    virtual void setCurrent(const Vec2u& point) = 0;
    virtual void unset(const Vec2u& point) = 0;
    virtual void reset(const Vec2u& point) = 0;
    virtual bool isSet(const Vec2u& index) const = 0;
    virtual void scaleValueAt(const Vec2u& index, int axis, float scale) = 0;
    virtual void extrapolateValueAt(const Vec2u& index, int axis) = 0;
    virtual void copyKeypointToBinding(const Vec2u& src, const std::shared_ptr<ParameterBinding>& other, const Vec2u& dest) = 0;
    virtual void swapKeypointWithBinding(const Vec2u& src, const std::shared_ptr<ParameterBinding>& other, const Vec2u& dest) = 0;
    virtual void reverseAxis(uint32_t axis) = 0;
    virtual void reInterpolate() = 0;
    virtual common::SharedArray<common::SharedArray<bool>>& getIsSet() = 0;
    virtual uint32_t getSetCount() const = 0;
    virtual void moveKeypoints(uint32_t axis, uint32_t oldindex, uint32_t newindex) = 0;
    virtual void insertKeypoints(uint32_t axis, uint32_t index) = 0;
    virtual void deleteKeypoints(uint32_t axis, uint32_t index) = 0;
    virtual uint32_t getNodeUUID() const = 0;
    virtual InterpolateMode interpolateMode() const = 0;
    virtual void setInterpolateMode(InterpolateMode mode) = 0;
    virtual BindTarget getTarget() const = 0;
    virtual std::shared_ptr<Node> getNode() const = 0;
    virtual std::shared_ptr<::nicxlive::core::Resource> getResourceTarget() const {
        return getTarget().getResource();
    }
    virtual void setResourceTarget(const std::shared_ptr<::nicxlive::core::Resource>& resource,
                                   common::NullableString name) = 0;
    virtual void setTarget(const std::shared_ptr<Node>& node, const std::string& paramName) = 0;
    virtual bool isCompatibleWithNode(const std::shared_ptr<Node>& other) const = 0;
    virtual bool isCompatibleWithResource(const std::shared_ptr<::nicxlive::core::Resource>& other) const {
        return isCompatibleWithNode(std::dynamic_pointer_cast<Node>(other));
    }
    virtual std::string getName() const = 0;
    // Canonical retained name result; getName is the legacy owned-string boundary.
    virtual common::NullableString getNameDescriptor() const { return getName(); }
    // Serialize / Deserialize
    virtual serde::ReadError serializeSelf(serde::TypedJsonWriter&) const {
        throw std::logic_error("Typed ParameterBinding writer is not implemented for this binding");
    }
    virtual serde::ReadError deserializeFromFghj(serde::TypedJsonView) {
        throw std::logic_error("Typed ParameterBinding reader is not implemented for this binding");
    }
    virtual ::nicxlive::core::serde::SerdeException serializeSelf(::nicxlive::core::serde::InochiSerializer&) const { return std::nullopt; }
    virtual ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj&) { return std::nullopt; }

    // Source Object defaults follow all pre-existing virtual declarations.
    const void* objectIdentity() const noexcept { return dynamic_cast<const void*>(this); }
    virtual std::size_t toHash() noexcept {
        const auto address = reinterpret_cast<std::size_t>(objectIdentity());
        return address ^ (address >> 4);
    }
    virtual bool opEquals(std::shared_ptr<ParameterBinding> other) {
        return other && objectIdentity() == other->objectIdentity();
    }
    virtual int opCmp(std::shared_ptr<ParameterBinding>) {
        ngRegisterModelResourceClassNames();
        std::string message = "need opCmp for class ";
        message += d_runtime::ngObjectClassName(typeid(*this)).view();
        throw d_runtime::ThrowableException(d_runtime::RetainedString(std::move(message)),
            d_runtime::RetainedString::fromStaticLiteral("object.d"), 195);
    }
    // Append the Source mutable protocol after all existing virtual slots.
    virtual serde::ReadError serializeSelf(serde::TypedJsonWriter&) = 0;
};

inline bool ngParameterBindingObjectEquals(std::shared_ptr<ParameterBinding> left,
    std::shared_ptr<ParameterBinding> right) {
    return common::ngClassObjectEquals(std::move(left), std::move(right), ngRegisterModelResourceClassNames);
}

} // namespace nicxlive::core::param

// Publish the actual concrete binding bodies after completing their interface.
#include "parameter.hpp"
