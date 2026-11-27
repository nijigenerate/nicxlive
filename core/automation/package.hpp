#pragma once

#include "../common/shared_array.hpp"
#include "../param/parameter.hpp"
#include "../compat/native_access_fault.hpp"
#include "../compat/retained_string.hpp"
#include "../common/nullable_string.hpp"
#include "../nodes/common.hpp"
#include "../serde_json.hpp"
#include "../serde_reader.hpp"
#include "../math/serialization.hpp"
#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace nicxlive::core {
void ngRegisterAutomationObjectClassNames();
class Puppet;
namespace param { class Parameter; }

struct AutomationBinding {
    std::optional<std::string> paramId;
    std::shared_ptr<param::Parameter> param;
    int axis = 0;
    nodes::Vec2 range{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};

    float getAxisValue() const;
    void setAxisValue(float value);
    void addAxisOffset(float value);
    template<class S> void serialize(S& serializer) {
        auto state = serializer.structBegin();
        serializer.putKey(common::NullableString::fromStaticLiteral("param"));
        const auto parameter = d_runtime::ngRequireObject(param);
        const auto name = parameter->getName();
        serializer.putValue(name);
        serializer.putKey(common::NullableString::fromStaticLiteral("axis"));
        serializer.putValue(axis);
        serializer.putKey(common::NullableString::fromStaticLiteral("range"));
        math::serialize(range, serializer);
        serializer.structEnd(state);
    }
    void serialize(serde::TypedJsonWriter& serializer) const;
    void reconstruct(const std::shared_ptr<Puppet>& puppet);
    void finalize(const std::shared_ptr<Puppet>& puppet);
    serde::ReadError deserializeFromFghj(serde::TypedJsonView data);
};

class Automation {
private:
    // The original class retains its parent; this is ordinary C++ ownership.
    std::shared_ptr<Puppet> parent;
protected:
    common::SharedArray<AutomationBinding> bindings;
    // Storage-only entry lets a derived Source constructor execute its authored
    // statements before the one actual base constructor body.
    struct SourceAutomationConstruction {};
    explicit Automation(SourceAutomationConstruction) {}
    void constructAutomationOwned(std::shared_ptr<Puppet> parentValue);
    float remapRange(float value, nodes::Vec2 range) const;
    virtual void onUpdate() {}
    virtual void serializeSelf(serde::TypedJsonWriter&) const {}
    virtual void deserializeSelf(serde::TypedJsonView) {}
public:
    std::optional<std::string> name;
    bool enabled = true;
    std::optional<std::string> typeId;

    explicit Automation(std::shared_ptr<Puppet> parent);
    virtual ~Automation() = default;
    virtual void bind(AutomationBinding binding);
    virtual void reconstruct(const std::shared_ptr<Puppet>& puppet);
    virtual void finalize(const std::shared_ptr<Puppet>& puppet) final;
    virtual void update() final;
    template<class S> void serialize(S& serializer) {
        auto state = serializer.structBegin();
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("type"));
        serializer.serializeValue(typeId);
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("name"));
        serializer.serializeValue(name);
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("bindings"));
        serializer.serializeValue(bindings);
        this->serializeSelf(serializer);
        serializer.structEnd(state);
    }
    void serialize(serde::TypedJsonWriter& serializer) const;
    virtual serde::ReadError deserializeFromFghj(serde::TypedJsonView data);
protected:
    virtual void serializeSelf(serde::TypedJsonWriter&) {}
public:
    // Inherited Object protocol; retain all existing native virtual slots.
    const void* objectIdentity() const noexcept { return dynamic_cast<const void*>(this); }
    virtual d_runtime::RetainedString toString();
    virtual std::size_t toHash() noexcept;
    virtual bool opEquals(std::shared_ptr<Automation> other);
    virtual int opCmp(std::shared_ptr<Automation> other);
};

inline serde::ReadError deserializeValue(serde::TypedJsonView data, AutomationBinding& value) {
    return value.deserializeFromFghj(data);
}

using AutomationFactory = std::function<std::shared_ptr<Automation>(std::shared_ptr<Puppet>)>;
// Specialize this trait to translate D's @TypeId annotation. Registration is
// explicit per thread, corresponding to D's thread-local static constructor.
template<class T> struct AutomationTypeId;
void inRegisterAutomationFactory(const std::string& id, AutomationFactory factory);
void inRegisterAutomationFactoryRetained(d_runtime::RetainedString id, AutomationFactory factory);
std::shared_ptr<Automation> inInstantiateAutomationRetained(d_runtime::RetainedString id,
    std::shared_ptr<Puppet> parent);
bool inHasAutomationTypeRetained(d_runtime::RetainedString id);
template<class T> requires std::is_convertible_v<T*, Automation*>
void inRegisterAutomationType() {
    // Source TypeId annotation records have static immutable byte storage.
    inRegisterAutomationFactoryRetained(
        d_runtime::RetainedString::fromStaticView(std::string_view(AutomationTypeId<T>::value)),
        [](std::shared_ptr<Puppet> puppet) {
            return std::make_shared<T>(puppet);
        });
}
std::shared_ptr<Automation> inInstantiateAutomation(const std::string& id, std::shared_ptr<Puppet> parent);
bool inHasAutomationType(const std::string& id);
} // namespace nicxlive::core
