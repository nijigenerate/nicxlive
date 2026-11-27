#pragma once

#include "package.hpp"
#include "sine_math.hpp"
#include "../timing.hpp"
#include "../compat/assert_error.hpp"
#include <cmath>
#include <stdexcept>
#include <string_view>

namespace nicxlive::core {
enum class SineType { Sin, Cos, Tan };

class SineAutomation : public Automation {
protected:
    void onUpdate() override {
        auto capturedBindings = bindings;
        for (auto& binding : capturedBindings) {
            float wave;
            switch (sineType) {
            case SineType::Sin:
                wave = remapRange(static_cast<float>(
                    (std::sin(currentTime() * speed + phase) + 1.0) / 2.0f), binding.range);
                break;
            case SineType::Cos:
                wave = remapRange(static_cast<float>(
                    (std::cos(currentTime() * speed + phase) + 1.0) / 2.0f), binding.range);
                break;
            case SineType::Tan:
                wave = remapRange(static_cast<float>(
                    (automation_detail::sineTan(currentTime() * speed + phase) + 1.0) / 2.0f), binding.range);
                break;
            default:
                d_runtime::ngAssertUnreachable("Assertion failure", "nijilive/core/automation/sine.d", 36);
            }
            binding.addAxisOffset(wave);
        }
    }
    void serializeSelf(serde::TypedJsonWriter& serializer) override {
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("speed")); serializer.putValue(speed);
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("sine_type"));
        serializer.putValue(static_cast<int>(sineType));
    }
    void serializeSelf(serde::TypedJsonWriter& serializer) const override {
        serializer.putKey("speed"); serializer.putValue(speed);
        serializer.putKey("sine_type"); serializer.putValue(static_cast<int>(sineType));
    }
    void deserializeSelf(serde::TypedJsonView data) override {
        (void)serde::deserializeValue(data.findFirst("speed"), speed);
        const auto typeData = data.findFirst("sine_type");
        std::string_view text;
        const auto kind = typeData.kind();
        if (kind == serde::JsonKind::String) text = typeData.stringBytes();
        else if (kind != serde::JsonKind::Null) (void)serde::unexpectedJsonKind(typeData);
        constexpr std::string_view names[]{"Sin", "Cos", "Tan"};
        for (unsigned i = 0; i < std::size(names); ++i) {
            if (text == names[i]) { sineType = static_cast<SineType>(i); return; }
        }
        std::string message = "Unable to deserialize string '";
        if (!text.empty()) message.append(text);
        message += "' to SineTypeAllowed keys:allowedKeys";
        throw d_runtime::ThrowableException(d_runtime::RetainedString(std::move(message)),
            d_runtime::RetainedString::fromStaticLiteral("fghj/serialization.d"), 2167);
    }
public:
    float speed = 1.0f;
    float phase = 0.0f;
    SineType sineType = SineType::Sin;
    explicit SineAutomation(std::shared_ptr<Puppet> parent) : Automation(SourceAutomationConstruction{}) {
        typeId = "sine";
        constructAutomationOwned(std::move(parent));
    }
};
template<> struct AutomationTypeId<SineAutomation> { static constexpr std::string_view value = "sine"; };
} // namespace nicxlive::core
