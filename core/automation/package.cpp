#include "package.hpp"
#include "sine.hpp"
#include "phys.hpp"
#include "../param/parameter.hpp"
#include "../puppet.hpp"
#include "../compat/native_access_fault.hpp"
#include "../compat/assert_error.hpp"
#include <array>
#include <stdexcept>
#include <string_view>
#include "../common/string_key_map.hpp"
#include "../compat/range_error.hpp"
#include <utility>
#include <mutex>
#include "../compat/object_metadata.hpp"
#include "../compat/throwable.hpp"

namespace nicxlive::core {
// Object protocol adapted from object.d, Digital Mars 2000-2011, BSL-1.0.
// See core/licenses/druntime-associative-array.txt.
void ngRegisterAutomationObjectClassNames() {
    static std::once_flag initialized;
    std::call_once(initialized, [] {
        d_runtime::ngRegisterObjectClassName(typeid(Automation),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.automation.Automation"));
        d_runtime::ngRegisterObjectClassName(typeid(SineAutomation),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.automation.sine.SineAutomation"));
        d_runtime::ngRegisterObjectClassName(typeid(PhysicsAutomation),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.automation.phys.PhysicsAutomation"));
    });
}
d_runtime::RetainedString Automation::toString() {
    ngRegisterAutomationObjectClassNames();
    return d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t Automation::toHash() noexcept {
    const auto address = reinterpret_cast<std::size_t>(objectIdentity());
    return address ^ (address >> 4);
}
bool Automation::opEquals(std::shared_ptr<Automation> other) {
    return other && objectIdentity() == other->objectIdentity();
}
int Automation::opCmp(std::shared_ptr<Automation>) {
    ngRegisterAutomationObjectClassNames();
    std::string message = "need opCmp for class ";
    message += d_runtime::ngObjectClassName(typeid(*this)).view();
    throw d_runtime::ThrowableException(d_runtime::RetainedString(std::move(message)),
        d_runtime::RetainedString::fromStaticLiteral("object.d"), 195);
}
namespace {
// D InAutomation module constructors register each concrete type per thread.
using Factory = std::shared_ptr<AutomationFactory>;
thread_local common::StringKeyMap<Factory> typeFactories = [] {
    common::StringKeyMap<Factory> registry;
    registry.setRetained(d_runtime::RetainedString::fromStaticLiteral("sine"),
        std::make_shared<AutomationFactory>([](std::shared_ptr<Puppet> parent) {
            return std::make_shared<SineAutomation>(parent);
        }));
    registry.setRetained(d_runtime::RetainedString::fromStaticLiteral("physics"),
        std::make_shared<AutomationFactory>([](std::shared_ptr<Puppet> parent) {
            return std::make_shared<PhysicsAutomation>(parent);
        }));
    return registry;
}();
} // namespace

float AutomationBinding::getAxisValue() const {
    switch (axis) {
    case 0: return d_runtime::ngRequireObject(param)->value.x;
    case 1: return d_runtime::ngRequireObject(param)->value.y;
    default: return std::numeric_limits<float>::quiet_NaN();
    }
}
void AutomationBinding::setAxisValue(float value) {
    switch (axis) {
    case 0: {
        const auto parameter = param;
        if (!parameter) d_runtime::ngFailNullAccess(true);
        parameter->value.x = value;
        break;
    }
    case 1: {
        const auto parameter = param;
        if (!parameter) d_runtime::ngFailNullAccess(true);
        parameter->value.y = value;
        break;
    }
    default:
        d_runtime::ngAssertUnreachable("Assertion failure", "nijilive/core/automation/package.d", 64);
    }
}
void AutomationBinding::addAxisOffset(float value) {
    d_runtime::ngRequireObject(param)->pushIOffsetAxis(axis, value);
}
void AutomationBinding::serialize(serde::TypedJsonWriter& serializer) const {
    const auto state = serializer.structBegin();
    serializer.putKey("param");
    const auto parameter = d_runtime::ngRequireObject(param);
    serializer.putValue(parameter->getName().view());
    serializer.putKey("axis");
    serializer.putValue(axis);
    serializer.putKey("range");
    serializer.serializeValue(std::array<float, 2>{range.x, range.y});
    serializer.structEnd(state);
}
void AutomationBinding::reconstruct(const std::shared_ptr<Puppet>&) {}
void AutomationBinding::finalize(const std::shared_ptr<Puppet>& puppet) {
    const auto owner = d_runtime::ngRequireObject(puppet);
    auto parameters = owner->parameters;
    for (auto& parameter : parameters) {
        const auto name = d_runtime::ngRequireObject(parameter)->getName();
        const auto expected = paramId ? std::string_view(*paramId) : std::string_view{};
        if (name.view() == expected) {
            param = parameter;
            return;
        }
    }
}
Automation::Automation(std::shared_ptr<Puppet> parentValue) {
    constructAutomationOwned(std::move(parentValue));
}
void Automation::constructAutomationOwned(std::shared_ptr<Puppet> parentValue) {
    parent = std::move(parentValue);
}
float Automation::remapRange(float value, nodes::Vec2 range) const {
    return range.x + value * (range.y - range.x);
}
void Automation::bind(AutomationBinding binding) { bindings.push_back(std::move(binding)); }
void Automation::reconstruct(const std::shared_ptr<Puppet>&) {
    auto snapshot = bindings.duplicate();
    for (auto& binding : snapshot) binding.reconstruct(parent);
}
void Automation::finalize(const std::shared_ptr<Puppet>& puppet) {
    const auto parentValue = puppet;
    parent = parentValue;
    auto capturedBindings = bindings;
    for (auto& binding : capturedBindings) binding.finalize(parentValue);
}
void Automation::update() { if (enabled) onUpdate(); }
void Automation::serialize(serde::TypedJsonWriter& serializer) const {
    const auto state = serializer.structBegin();
    serializer.putKey("type"); serializer.serializeValue(typeId);
    serializer.putKey("name"); serializer.serializeValue(name);
    serializer.putKey("bindings"); serializer.serializeValue(bindings);
    serializeSelf(serializer);
    serializer.structEnd(state);
}
serde::ReadError AutomationBinding::deserializeFromFghj(serde::TypedJsonView data) {
    (void)serde::deserializeValue(data.findFirst("param"), paramId);
    (void)serde::deserializeValue(data.findFirst("axis"), axis);
    // Original D reads the range from axis, not the range key.
    (void)math::deserialize(range, data.findFirst("axis"));
    return {};
}
serde::ReadError Automation::deserializeFromFghj(serde::TypedJsonView data) {
    (void)serde::deserializeValue(data.findFirst("name"), name);
    (void)serde::deserializeValue(data.findFirst("bindings"), bindings);
    deserializeSelf(data);
    return {};
}
void inRegisterAutomationFactoryRetained(d_runtime::RetainedString id, AutomationFactory factory) {
    typeFactories.setRetained(id, std::make_shared<AutomationFactory>(std::move(factory)));
}
void inRegisterAutomationFactory(const std::string& id, AutomationFactory factory) {
    inRegisterAutomationFactoryRetained(d_runtime::RetainedString(id), std::move(factory));
}
std::shared_ptr<Automation> inInstantiateAutomationRetained(d_runtime::RetainedString id,
    std::shared_ptr<Puppet> parent) {
    const auto entry = typeFactories.findRetained(id.view());
    if (!entry) throw d_runtime::ngStaticError<d_runtime::RangeError>(
        d_runtime::RetainedString::fromStaticLiteral("nijilive/core/automation/package.d"), 244);
    const auto factory = *entry;
    if (!factory || !*factory) d_runtime::ngFailNullAccess(false);
    return (*factory)(parent);
}
std::shared_ptr<Automation> inInstantiateAutomation(const std::string& id, std::shared_ptr<Puppet> parent) {
    return inInstantiateAutomationRetained(d_runtime::RetainedString::fromBorrowedView(id), parent);
}
bool inHasAutomationTypeRetained(d_runtime::RetainedString id) {
    return typeFactories.contains(id.view());
}
bool inHasAutomationType(const std::string& id) {
    return inHasAutomationTypeRetained(d_runtime::RetainedString::fromBorrowedView(id));
}
} // namespace nicxlive::core
