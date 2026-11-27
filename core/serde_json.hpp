#pragma once

#include "serialization_interface.hpp"
#include "serialization_fields.hpp"
#include "compat/system_error.hpp"
#include "compat/static_error.hpp"
#include "common/string_key_map.hpp"
#include "common/nullable_string.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace nicxlive::core::serde {

// Compact event writer matching nijilive's InochiSerializer and fghj 1.0.2
// default (non-SSE42) escaping. This does not reinterpret a property_tree:
// callers supply the scalar types, nullness and field order explicitly.
class TypedJsonWriter {
    // Only primitive string storage is lowered here. User serialization and
    // numeric conversion execute at their original call sites outside catches.
    template<class Build> static void mutateBytes(Build&& build) {
        try { std::forward<Build>(build)(); }
        catch (const std::bad_alloc&) {
            throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
        }
    }
    static void appendBytes(std::string& output, char value) {
        mutateBytes([&] { output.push_back(value); });
    }
    template<class... Args> requires requires(std::string& output, Args&&... values) {
        output.append(std::forward<Args>(values)...);
    }
    static void appendBytes(std::string& output, Args&&... values) {
        mutateBytes([&] { output.append(std::forward<Args>(values)...); });
    }
    template<class... Args> void appendText(Args&&... values) {
        appendBytes(text_, std::forward<Args>(values)...);
    }
public:
    using State = std::uint32_t;

    State structBegin(std::size_t = 0) { appendText('{'); return popState(); }
    State listBegin(std::size_t = 0) { appendText('['); return popState(); }
    void structEnd(State state) { appendText('}'); state_ = state; }
    void listEnd(State state) { appendText(']'); state_ = state; }
    void elemBegin() { incrementState(); }

    void putKey(std::string_view key) {
        incrementState();
        putValue(key);
        appendText(':');
    }
    // Retain the actual immutable descriptor throughout primitive output.
    template<class T> requires requires(const T& value) {
        { value.view() } -> std::convertible_to<std::string_view>;
        value.retentionOwner();
    }
    void putKey(T value) { putKey(std::string_view(value.view())); }
    // As in fghj, this key is already escaped by the metadata producer.
    void putEscapedKey(std::string_view key) {
        if (key.size() > 255) throw std::length_error("Escaped JSON key exceeds 255 bytes");
        incrementState();
        appendText('"');
        appendText(key);
        appendText("\":");
    }
    void putValue(std::nullptr_t) { appendText("null"); }
    void putValue(bool value) { appendText(value ? "true" : "false"); }
    void putValue(std::string_view value) {
        appendText('"');
        static constexpr char hex[] = "0123456789ABCDEF";
        for (unsigned char byte : value) {
            switch (byte) {
            case '\b': appendText("\\b"); break;
            case '\f': appendText("\\f"); break;
            case '\n': appendText("\\n"); break;
            case '\r': appendText("\\r"); break;
            case '\t': appendText("\\t"); break;
            case '\\': appendText("\\\\"); break;
            case '"': appendText("\\\""); break;
            default:
                if (byte < 0x20 || byte == 0xff) {
                    appendText("\\u00");
                    appendText(hex[byte >> 4]);
                    appendText(hex[byte & 15]);
                } else appendText(static_cast<char>(byte));
            }
        }
        appendText('"');
    }
    void putValue(const char* value) { putValue(value ? std::string_view(value) : std::string_view{}); }
    void putValue(const std::string& value) { putValue(std::string_view(value)); }
    template<class T> requires requires(const T& value) {
        { value.view() } -> std::convertible_to<std::string_view>;
        value.retentionOwner();
    }
    void putValue(T value) { putValue(std::string_view(value.view())); }

    template<std::integral T> requires (!std::same_as<T, bool>)
    void putNumberValue(T value) {
        char buffer[std::numeric_limits<T>::digits10 + 4];
        const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);
        if (result.ec != std::errc{}) throw std::runtime_error("Integer formatting failed");
        appendText(buffer, result.ptr);
    }
    template<class T> requires (std::same_as<T, float> || std::same_as<T, double>)
    void putNumberValue(T value) {
        if (std::isnan(value)) { appendText("\"nan\""); return; }
        if (std::isinf(value)) { appendText(std::signbit(value) ? "\"-inf\"" : "\"+inf\""); return; }
        if (value == 0) { appendText(std::signbit(value) ? "-0.0" : "0.0"); return; }
        // Obtain the shortest round-tripping coefficient. The notation below
        // follows mir.bignum.decimal.NumericSpec.human rather than printf %g.
        char buffer[64];
        const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value,
                                          std::chars_format::scientific);
        if (result.ec != std::errc{}) throw std::runtime_error("Floating formatting failed");
        std::string_view number(buffer, result.ptr - buffer);
        if (number.front() == '-') { appendText('-'); number.remove_prefix(1); }
        const auto marker = number.find('e');
        std::string digits;
        for (char c : number.substr(0, marker)) if (c != '.') appendBytes(digits, c);
        auto exponentText = number.substr(marker + 1);
        bool negative = exponentText.front() == '-';
        exponentText.remove_prefix(1);
        int scientificExponent = 0;
        for (char c : exponentText) scientificExponent = scientificExponent * 10 + c - '0';
        if (negative) scientificExponent = -scientificExponent;
        int exponent = scientificExponent - static_cast<int>(digits.size()) + 1;
        while (digits.size() > 1 && digits.back() == '0') { digits.pop_back(); ++exponent; }
        const int split = exponent + static_cast<int>(digits.size());
        if (split <= 0 && (exponent >= -6 || split >= -2)) {
            appendText("0.");
            appendText(static_cast<std::size_t>(-split), '0');
            appendText(digits);
        } else if (split > 0 && exponent >= 0 && split <= 6) {
            appendText(digits);
            appendText(static_cast<std::size_t>(exponent), '0');
            appendText(".0");
        } else if (split > 0 && exponent < 0 && (split <= 6 || digits.size() <= 6)) {
            appendText(digits, 0, static_cast<std::size_t>(split));
            appendText('.');
            appendText(digits, static_cast<std::size_t>(split));
        } else {
            appendText(digits.front());
            if (digits.size() > 1) { appendText('.'); appendText(digits, 1); }
            appendText(scientificExponent < 0 ? "e-" : "e+");
            putNumberValue(scientificExponent < 0 ? -scientificExponent : scientificExponent);
        }
    }
    template<class T> requires ((std::integral<T> && !JsonCharacter<T>) ||
        std::same_as<T, float> || std::same_as<T, double>)
    void putValue(T value) {
        if constexpr (std::same_as<T, bool>) putValue(static_cast<bool>(value));
        else putNumberValue(value);
    }
    template<class T> requires (JsonCharacter<T> || std::is_enum_v<T>)
    void putValue(T) = delete;

    void serializeValue(std::nullptr_t value) { putValue(value); }
    // An absent string-view descriptor is D's null string; putValue(view)
    // intentionally does not do this check, matching D's primitive overload.
    void serializeValue(std::string_view value) {
        if (!value.data()) putValue(nullptr);
        else putValue(value);
    }
    void serializeValue(const std::string& value) { putValue(value); }
    void serializeValue(d_runtime::RetainedString value) {
        if (value.isNull()) putValue(nullptr);
        else putValue(value);
    }
    void serializeValue(common::NullableString value) {
        if (value.isNull()) putValue(nullptr);
        else putValue(value);
    }
    template<class T> requires DeclaredJsonString<T>
    void serializeValue(const T& value) {
        auto descriptor = SourceJsonString<std::remove_cvref_t<T>>::read(value);
        serializeValue(std::move(descriptor));
    }
    // The authoritative fghj char overload does not instantiate under LDC
    // 1.41; do not silently introduce a numeric or string interpretation.
    void serializeValue(char) = delete;
    void serializeValue(const char* value) {
        if (!value) putValue(nullptr);
        else putValue(std::string_view(value));
    }
    template<class T> requires JsonCharacter<T>
    void serializeValue(T) = delete;
    template<class T> requires ((std::integral<T> && !JsonCharacter<T>) ||
        std::same_as<T, float> || std::same_as<T, double>)
    void serializeValue(T value) { putValue(value); }
    template<class T> requires DeclaredJsonEnum<T, TypedJsonWriter>
    void serializeValue(T value) { SourceJsonEnum<std::remove_cvref_t<T>>::serialize(value, *this); }
    template<class T> void serializeValue(const std::optional<T>& value) {
        if (value) serializeValue(*value);
        else putValue(nullptr);
    }
    template<class T> requires (!DeclaredSourcePointer<std::shared_ptr<T>>)
    void serializeValue(const std::shared_ptr<T>& value) {
        const auto receiver = value;
        if (receiver) {
            if constexpr (fmt::CustomSerializable<T>)
                fmt::ngSerializeCustom(*receiver, *this);
            else if constexpr (requires(T& object, TypedJsonWriter& writer) { object.serialize(writer); })
                receiver->serialize(*this);
            else serializeValue(*receiver);
        }
        else putValue(nullptr);
    }
    template<class T, std::size_t Extent> void serializeValue(std::span<T, Extent> values) {
        if constexpr (Extent == std::dynamic_extent) {
            if (!values.data()) { putValue(nullptr); return; }
        }
        putArray(values);
    }
    template<class T, std::size_t N> void serializeValue(const std::array<T, N>& values) { putArray(values); }
    template<class T> void serializeValue(const std::vector<T>& values) { putArray(values); }
    // fghj serialization.d:1567: string-key AAs are objects and expose their
    // actual mapped values by reference during custom serialization.
    template<class T> void serializeValue(const common::StringKeyMap<T>& values) {
        auto retained = values;
        if (retained.isNull()) { putValue(nullptr); return; }
        auto state = structBegin();
        retained.forEachEntry([&](const auto& storedKey, auto& value) {
            auto key = storedKey;
            putKey(key);
            serializeValue(value);
        });
        structEnd(state);
    }
    template<class T, class Hash, class Equal>
    void serializeValue(const common::ResourceKeyMap<d_runtime::RetainedString, T, Hash, Equal>& values) {
        auto retained = values;
        if (retained.isNull()) { putValue(nullptr); return; }
        auto state = structBegin();
        for (auto& entry : retained) {
            auto key = entry.first;
            putKey(key);
            serializeValue(entry.second);
        }
        structEnd(state);
    }
    template<class T> requires requires(const T& value) { value.isNull(); value.begin(); value.end(); }
    void serializeValue(const T& values) {
        if constexpr (DeclaredJsonEnumMap<T, TypedJsonWriter>) {
            SourceJsonEnumMap<std::remove_cvref_t<T>>::serialize(values, *this);
        } else if constexpr (DeclaredJsonArray<T, TypedJsonWriter>) {
            SourceJsonArray<std::remove_cvref_t<T>>::serialize(values, *this);
        } else {
            auto retained = values;
            if (retained.isNull()) putValue(nullptr);
            else putArrayElements(retained);
        }
    }
    template<class T> requires (DeclaredJsonSerializable<T, TypedJsonWriter> &&
        !requires(T& value, TypedJsonWriter& writer) { value.serialize(writer); })
    void serializeValue(T& value) {
        SourceJsonFields<std::remove_cvref_t<T>>::serialize(value, *this);
    }
    template<class T> requires (!std::is_const_v<T> &&
        requires(T& value, TypedJsonWriter& writer) { value.serialize(writer); } &&
        !requires(const T& value) { value.isNull(); value.begin(); value.end(); })
    void serializeValue(T& value) {
        if constexpr (fmt::CustomSerializable<T>) fmt::ngSerializeCustom(value, *this);
        else value.serialize(*this);
    }
    template<class T> requires requires(const T& value, TypedJsonWriter& writer) { value.serialize(writer); }
    void serializeValue(const T& value) { value.serialize(*this); }
    template<class Range> void putArray(const Range& values) {
        if constexpr (requires { values.isNull(); values.begin(); values.end(); }) {
            auto retained = values;
            putArrayElements(retained);
        } else putArrayElements(values);
    }
    // D serdeOptional affects missing-field reads, not output omission.
    template<class T> void putField(std::string_view key, const T& value) {
        putKey(key);
        serializeValue(value);
    }
    template<class Key, class T>
        requires std::same_as<std::remove_cvref_t<Key>, d_runtime::RetainedString>
    void putField(Key key, const T& value) {
        putKey(key);
        serializeValue(value);
    }
    const std::string& str() const { return text_; }
    std::string takeString() && { return std::move(text_); }
    // String accumulation has no external buffered sink to drain.
    void flush() const {}

private:
    template<class Range> void putArrayElements(Range& values) {
        const auto state = listBegin();
        for (auto&& value : values) { elemBegin(); serializeValue(value); }
        listEnd(state);
    }
    State popState() { auto result = state_; state_ = 0; return result; }
    void incrementState() { if (state_++) appendText(','); }
    State state_ = 0;
    std::string text_;
};
} // namespace nicxlive::core::serde
