#pragma once

#include "common/shared_array.hpp"
#include "compat/retained_string.hpp"
#include "serde_source_exceptions.hpp"
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <iterator>
#include <memory>
#include <optional>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace nicxlive::core::serde {
enum class JsonKind { Null, Boolean, Number, String, Array, Object };
// A nullable owner of the genuine source exception, never a message identity.
class ReadError {
public:
    using Object = source_exceptions::SerdeException;

    ReadError() noexcept = default;
    ReadError(std::nullptr_t) noexcept {}
    ReadError(std::nullopt_t) noexcept {}
    ReadError(std::shared_ptr<Object> object) noexcept : object_(std::move(object)) {}

    // Existing native-only message producers keep their spelling. Source
    // producers pass the actual exception owner through the constructor above.
    ReadError(std::string message, std::source_location source = std::source_location::current())
        : object_(std::make_shared<Object>(d_runtime::RetainedString(std::move(message)),
            d_runtime::RetainedString(source.file_name()), source.line())) {}
    ReadError(const char* message, std::source_location source = std::source_location::current())
        : ReadError(std::string(message), source) {}

    explicit operator bool() const noexcept { return static_cast<bool>(object_); }
    bool has_value() const noexcept { return static_cast<bool>(*this); }
    Object* get() const noexcept { return object_.get(); }
    Object* operator->() const noexcept { return get(); }
    const std::shared_ptr<Object>& object() const noexcept { return object_; }

    // Compatibility message extraction does not replace the retained object.
    std::string operator*() const { return std::string(object_->msg.view()); }
    std::string value() const {
        if (!object_) throw std::bad_optional_access();
        return **this;
    }
    void reset() noexcept { object_.reset(); }
    friend bool operator==(const ReadError&, const ReadError&) noexcept = default;
    friend bool operator==(const ReadError& value, std::nullptr_t) noexcept { return !value; }
    friend bool operator==(const ReadError& value, std::nullopt_t) noexcept { return !value; }

private:
    std::shared_ptr<Object> object_;
};
// Compatibility spellings of actual source carriers, not invented D classes.
using MissingJsonValue = source_exceptions::EmptyFghjException;
using JsonConversionError = d_runtime::ThrowableException;
using JsonParseError = source_exceptions::FghjSerdeException;
namespace detail { struct TypedJsonNode; }

class TypedJsonElementRange;
class TypedJsonMemberRange;

class TypedJsonView {
public:
    TypedJsonView() = default;
    explicit TypedJsonView(std::shared_ptr<const detail::TypedJsonNode> node) : node_(std::move(node)) {}
    bool isMissing() const noexcept { return !node_; }
    JsonKind kind() const;
    TypedJsonView findFirst(std::string_view key) const;
    std::vector<TypedJsonView> elements() const;
    TypedJsonElementRange byElement() const noexcept;
    std::vector<std::pair<std::string, TypedJsonView>> members() const;
    TypedJsonMemberRange byKeyValue() const noexcept;
    std::string_view numberToken() const;
    std::string_view stringBytes() const;
    bool boolean() const;
private:
    friend ReadError deserializeValue(TypedJsonView data, std::array<float, 2>& value);
    template<class T> friend ReadError deserializeVector(TypedJsonView data, std::span<T> value);
    std::span<const std::shared_ptr<detail::TypedJsonNode>> elementNodes() const noexcept;
    std::shared_ptr<const detail::TypedJsonNode> node_;
};
// FGHJ input range over the existing immutable graph, not an element snapshot.
class TypedJsonElementRange {
public:
    class Iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = TypedJsonView;
        using reference = TypedJsonView;
        using pointer = void;
        using iterator_category = std::input_iterator_tag;
        Iterator() noexcept = default;
        Iterator(const Iterator&) noexcept = default;
        Iterator& operator=(const Iterator&) noexcept = default;
        Iterator(Iterator&& other) noexcept : Iterator(static_cast<const Iterator&>(other)) {}
        Iterator& operator=(Iterator&& other) noexcept {
            return *this = static_cast<const Iterator&>(other);
        }
        TypedJsonView operator*() const;
        Iterator& operator++() noexcept;
        Iterator operator++(int) noexcept { auto old = *this; ++*this; return old; }
        friend bool operator==(const Iterator&, const Iterator&) noexcept = default;
    private:
        friend class TypedJsonElementRange;
        Iterator(std::shared_ptr<const detail::TypedJsonNode> node, std::size_t index) noexcept
            : node_(std::move(node)), index_(index) {}
        std::shared_ptr<const detail::TypedJsonNode> node_;
        std::size_t index_ = 0;
    };

    TypedJsonElementRange() noexcept = default;
    TypedJsonElementRange(const TypedJsonElementRange&) noexcept = default;
    TypedJsonElementRange& operator=(const TypedJsonElementRange&) noexcept = default;
    TypedJsonElementRange(TypedJsonElementRange&& other) noexcept
        : TypedJsonElementRange(static_cast<const TypedJsonElementRange&>(other)) {}
    TypedJsonElementRange& operator=(TypedJsonElementRange&& other) noexcept {
        return *this = static_cast<const TypedJsonElementRange&>(other);
    }
    bool empty() const noexcept;
    TypedJsonView front() const;
    void popFront() noexcept;
    TypedJsonElementRange save() const noexcept { return *this; }
    Iterator begin() const noexcept;
    Iterator end() const noexcept;
private:
    friend class TypedJsonView;
    explicit TypedJsonElementRange(std::shared_ptr<const detail::TypedJsonNode> node,
        std::size_t index = 0) noexcept : node_(std::move(node)), index_(index) {}
    std::shared_ptr<const detail::TypedJsonNode> node_;
    std::size_t index_ = 0;
};

// FGHJ's key/value tuple borrows counted key bytes from the actual JSON owner.
class TypedJsonMemberRange {
public:
    using Value = std::pair<d_runtime::RetainedString, TypedJsonView>;
    class Iterator;
    TypedJsonMemberRange() noexcept = default;
    TypedJsonMemberRange(const TypedJsonMemberRange&) noexcept = default;
    TypedJsonMemberRange& operator=(const TypedJsonMemberRange&) noexcept = default;
    TypedJsonMemberRange(TypedJsonMemberRange&& other) noexcept
        : TypedJsonMemberRange(static_cast<const TypedJsonMemberRange&>(other)) {}
    TypedJsonMemberRange& operator=(TypedJsonMemberRange&& other) noexcept {
        return *this = static_cast<const TypedJsonMemberRange&>(other);
    }
    bool empty() const noexcept;
    Value front() const;
    void popFront() noexcept;
    TypedJsonMemberRange save() const noexcept { return *this; }
    Iterator begin() const noexcept;
    Iterator end() const noexcept;
private:
    friend class TypedJsonView;
    explicit TypedJsonMemberRange(std::shared_ptr<const detail::TypedJsonNode> node,
        std::size_t index = 0) noexcept : node_(std::move(node)), index_(index) {}
    std::shared_ptr<const detail::TypedJsonNode> node_;
    std::size_t index_ = 0;
};

class TypedJsonMemberRange::Iterator {
public:
    using difference_type = std::ptrdiff_t;
    using value_type = Value;
    using reference = Value;
    using pointer = void;
    using iterator_category = std::input_iterator_tag;
    Iterator() noexcept = default;
    Value operator*() const { return range_.front(); }
    Iterator& operator++() noexcept { range_.popFront(); return *this; }
    Iterator operator++(int) noexcept { auto old = *this; ++*this; return old; }
    bool operator==(const Iterator& other) const noexcept {
        return range_.node_ == other.range_.node_ && range_.index_ == other.range_.index_;
    }
private:
    friend class TypedJsonMemberRange;
    explicit Iterator(TypedJsonMemberRange range) noexcept : range_(std::move(range)) {}
    TypedJsonMemberRange range_;
};

class TypedJsonDocument {
public:
    explicit TypedJsonDocument(TypedJsonView root) : root_(std::move(root)) {}
    TypedJsonView root() const { return root_; }
private:
    TypedJsonView root_;
};
TypedJsonDocument parseTypedJson(std::string_view input);
ReadError unexpectedJsonKind(JsonKind kind);
ReadError unexpectedJsonKind(TypedJsonView data);
ReadError deserializeValue(TypedJsonView data, bool& value);
ReadError deserializeValue(TypedJsonView data, std::optional<std::string>& value);
// Plain string destinations preserve contents but cannot represent D nullness.
ReadError deserializeValue(TypedJsonView data, std::string& value);
// Ordinary Source string decoding duplicates bytes; empty .dup is null.
ReadError deserializeValue(TypedJsonView data, d_runtime::RetainedString& value);
ReadError deserializeValue(TypedJsonView data, float& value);
ReadError deserializeValue(TypedJsonView data, double& value);
ReadError deserializeValue(TypedJsonView data, std::array<float, 2>& value);
ReadError deserializeNumber(TypedJsonView data, float& value);
ReadError deserializeNumber(TypedJsonView data, double& value);

template<class T> requires (std::is_integral_v<T> && !std::is_same_v<T, bool>)
ReadError deserializeValue(TypedJsonView data, T& value) {
    if (data.kind() != JsonKind::Number) return unexpectedJsonKind(data);
    auto text = data.numberToken();
    // Mir accepts a leading plus for both signed and unsigned integral values.
    if (!text.empty() && text.front() == '+') {
        text.remove_prefix(1);
        if (text.empty() || text.front() < '0' || text.front() > '9')
            detail::throwMirFromStringFailure<T>();
    }
    T result{};
    auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{}) detail::throwMirFromStringFailure<T>();
    if (parsed.ptr != text.data() + text.size()) detail::throwMirFromStringFailure<T>(true);
    value = result;
    return {};
}

template<class T>
ReadError deserializeValue(TypedJsonView data, common::SharedArray<T>& value) {
    if (data.kind() == JsonKind::Null) {
        // Publish the complete null descriptor before releasing its old cells.
        [[maybe_unused]] const auto previous = value;
        value = common::SharedArray<T>{};
        return {};
    }
    if (data.kind() != JsonKind::Array) return unexpectedJsonKind(data);
    auto entries = data.byElement();
    std::size_t count = 0;
    for (auto remaining = entries.save(); !remaining.empty(); remaining.popFront()) ++count;
    common::SharedArray<T> replacement{std::vector<T>(count)};
    if constexpr (std::is_floating_point_v<T>)
        for (auto& element : replacement) element = std::numeric_limits<T>::quiet_NaN();
    value = replacement;
    auto output = value;
    for (std::size_t i = 0; i < output.size(); ++i) {
        if (auto error = deserializeValue(entries.front(), output[i])) return error;
        entries.popFront();
    }
    return {};
}

// Explicit vector adapter preserves untouched trailing coordinates and prefix
// writes before conversion errors. Dynamic-array replacement is a separate API.
template<class T>
ReadError deserializeVector(TypedJsonView data, std::span<T> value) {
    if (data.isMissing() || data.kind() == JsonKind::Null) return {};
    if (data.kind() != JsonKind::Array) throw JsonConversionError(
        d_runtime::RetainedString(std::string_view("Expected a numeric vector array")),
        d_runtime::RetainedString::fromStaticLiteral("nijilive/math/serialization.d"), 36);
    const auto entries = data.elementNodes();
    for (std::size_t i = 0; i < entries.size() && i < value.size(); ++i)
        if (auto error = deserializeNumber(TypedJsonView(entries[i]), value[i])) return error;
    return {};
}
} // namespace nicxlive::core::serde
