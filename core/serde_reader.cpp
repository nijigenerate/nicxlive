#include "serde_reader.hpp"
#include "compat/assert_error.hpp"
#include "compat/system_error.hpp"
#include <new>
#include <algorithm>

#ifndef NICXLIVE_FGHJ_SOURCE_FILE
#define NICXLIVE_FGHJ_SOURCE_FILE "fghj/fghj.d"
#endif

#ifndef NICXLIVE_FGHJ_SERIALIZATION_SOURCE_FILE
#define NICXLIVE_FGHJ_SERIALIZATION_SOURCE_FILE "fghj/serialization.d"
#endif

namespace nicxlive::core::serde {
namespace detail {
struct TypedJsonNode {
    JsonKind kind = JsonKind::Null;
    bool boolean = false;
    std::string bytes;
    std::vector<std::shared_ptr<TypedJsonNode>> elements;
    std::vector<std::pair<std::string, std::shared_ptr<TypedJsonNode>>> members;
};
} // namespace detail
namespace {
using Node = detail::TypedJsonNode;
void requireKind(JsonKind actual, JsonKind expected) {
    if (actual != expected) throw JsonConversionError("Unexpected typed JSON kind");
}
template<class T> ReadError readFloating(TypedJsonView data, T& value, bool legacy) {
    if (data.kind() == JsonKind::Null) { value = std::numeric_limits<T>::quiet_NaN(); return {}; }
    if (data.kind() != JsonKind::Number && data.kind() != JsonKind::String) return unexpectedJsonKind(data);
    auto text = data.kind() == JsonKind::Number ? data.numberToken() : data.stringBytes();
    std::string normalized;
    if (legacy && text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        normalized.assign(text.substr(1, text.size() - 2));
        for (auto& c : normalized) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        if (normalized == "nan" || normalized == "+nan" || normalized == "-nan" ||
            normalized == "inf" || normalized == "+inf" || normalized == "-inf") text = normalized;
    }
    if (text == "nan" || text == "NaN" || text == "NAN" || text == "+nan" || text == "+NaN" ||
        text == "+NAN" || text == "-nan" || text == "-NaN" || text == "-NAN") {
        value = std::numeric_limits<T>::quiet_NaN(); return {};
    }
    if (text == "inf" || text == "Inf" || text == "INF" || text == "+inf" || text == "+Inf" || text == "+INF") {
        value = std::numeric_limits<T>::infinity(); return {};
    }
    if (text == "-inf" || text == "-Inf" || text == "-INF") { value = -std::numeric_limits<T>::infinity(); return {}; }
    std::string decimalText(text);
    for (auto& c : decimalText) if (c == 'd' || c == 'D') c = 'e';
    const auto exponentMarker = decimalText.find_first_of("eE");
    for (std::size_t i = 0; i < decimalText.size(); ++i) {
        if (decimalText[i] != '_') continue;
        if (i == 0 || i + 1 == decimalText.size() || i > exponentMarker ||
            decimalText[i-1] < '0' || decimalText[i-1] > '9' || decimalText[i+1] < '0' || decimalText[i+1] > '9')
            detail::throwMirFromStringFailure<T>();
    }
    decimalText.erase(std::remove(decimalText.begin(), decimalText.end(), '_'), decimalText.end());
    text = decimalText;
    T result{};
    if (!text.empty() && text.front() == '+') text.remove_prefix(1);
    for (const char c : text)
        if (!((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E'))
            detail::throwMirFromStringFailure<T>();
    std::int64_t decimalExponent = 0;
    if (const auto at = text.find_first_of("eE"); at != std::string_view::npos) {
        auto exponent = text.substr(at + 1);
        if (!exponent.empty() && exponent.front() == '+') exponent.remove_prefix(1);
        const auto converted = std::from_chars(exponent.data(), exponent.data() + exponent.size(), decimalExponent);
        if (converted.ec != std::errc{} || converted.ptr != exponent.data() + exponent.size())
            detail::throwMirFromStringFailure<T>();
    }
    const auto mantissaEnd = text.find_first_of("eE");
    const auto mantissaText = text.substr(0, mantissaEnd);
    if (const auto dot = mantissaText.find('.'); dot != std::string_view::npos) {
        const auto fraction = static_cast<std::int64_t>(mantissaText.size() - dot - 1);
        if (decimalExponent < std::numeric_limits<std::int64_t>::min() + fraction)
            detail::throwMirFromStringFailure<T>();
        decimalExponent -= fraction;
    }
    const bool negativeInput = !text.empty() && text.front() == '-';
    bool nonzeroCoefficient = false;
    for (char c : mantissaText) if (c >= '1' && c <= '9') nonzeroCoefficient = true;
    // Mir DecimalView reserves LONG_MAX as its special-value exponent even
    // when it was reached through a numeric token, rather than a named value.
    if (decimalExponent == std::numeric_limits<std::int64_t>::max()) {
        result = nonzeroCoefficient ? std::numeric_limits<T>::quiet_NaN() : std::numeric_limits<T>::infinity();
        value = negativeInput ? -result : result; return {};
    }
    // Match the current LDC x64 algorithmM conversion sentinel followed by
    // signed wrapping subtraction of mant_dig. Other D targets remain a gate.
    if (nonzeroCoefficient && decimalExponent < 0 &&
        static_cast<T>(-3.32192809488736234787031942948939018L) * static_cast<T>(decimalExponent) >= std::ldexp(T(1), 63)) {
        value = negativeInput ? -std::numeric_limits<T>::infinity() : std::numeric_limits<T>::infinity(); return {};
    }
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec == std::errc::result_out_of_range && parsed.ptr == text.data() + text.size()) {
        // Mir saturates valid decimal overflow to infinity and underflow to
        // signed zero. Determine magnitude without a narrower intermediate.
        const auto exponentAt = text.find_first_of("eE");
        auto mantissa = text.substr(0, exponentAt);
        long exponent = 0;
        if (exponentAt != std::string_view::npos) {
            auto exp = text.substr(exponentAt + 1); bool negative = false;
            if (!exp.empty() && (exp.front() == '-' || exp.front() == '+')) { negative = exp.front() == '-'; exp.remove_prefix(1); }
            for (const char c : exp) exponent = std::min(100000L, exponent * 10 + (c - '0'));
            if (negative) exponent = -exponent;
        }
        bool negative = !mantissa.empty() && mantissa.front() == '-';
        if (negative) mantissa.remove_prefix(1);
        const auto dot = mantissa.find('.');
        long position = static_cast<long>(dot == std::string_view::npos ? mantissa.size() : dot);
        long digits = 0; bool nonzero = false;
        for (char c : mantissa) { if (c == '.') continue; if (c != '0') { nonzero = true; break; } ++digits; }
        const bool overflow = nonzero && exponent + position - digits - 1 >= 0;
        result = overflow ? std::numeric_limits<T>::infinity() : T(0);
        value = negative ? -result : result; return {};
    }
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        detail::throwMirFromStringFailure<T>();
    value = result;
    return {};
}
} // namespace
TypedJsonDocument parseTypedJson(std::string_view input) {
    // The current FGHJ contiguous-input state machine feeds the existing real
    // typed-node graph. It reads the first value, not a complete JSON document.
    // No Boost diagnostic or second-reader fallback replaces source failures.
    struct Frame {
        std::shared_ptr<Node> node;
        std::string key;
        std::uint64_t contentBytes = 0;
    };
    enum class Step { Value, ObjectFirst, ArrayFirst, Key, AfterValue };
    std::vector<Frame> stack;
    std::shared_ptr<Node> root;
    std::size_t cursor = 0;
    Step step = Step::Value;
    bool done = false;
    const auto fail = [&](std::string_view message) -> void {
        throw JsonParseError(message, cursor);
    };
    const auto skipSpaces = [&] {
        while (cursor < input.size() && (input[cursor] == ' ' || input[cursor] == '\t' ||
                input[cursor] == '\r' || input[cursor] == '\n')) ++cursor;
    };
    const auto readHex = [&]() -> std::uint32_t {
        std::uint32_t result = 0;
        for (unsigned i = 0; i != 4; ++i) {
            if (cursor == input.size()) fail("unexpected end when parsing string");
            const auto c = static_cast<unsigned char>(input[cursor++]);
            unsigned value;
            if (c >= '0' && c <= '9') value = c - '0';
            else if (c >= 'a' && c <= 'f') value = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') value = c - 'A' + 10;
            else { fail("unexpected character when parsing string"); return 0; }
            result = (result << 4) | value;
        }
        return result;
    };
    const auto readString = [&](bool key) {
        std::string result;
        ++cursor;
        for (;;) {
            if (cursor == input.size()) fail("unexpected end when parsing string");
            const auto c = static_cast<unsigned char>(input[cursor]);
            if (c == '"') {
                ++cursor;
                if (key && result.size() > 255) fail("key length is limited to 255 characters");
                if (!key && result.size() > std::numeric_limits<std::uint32_t>::max())
                    fail("string size is limited to 2^32-1");
                return result;
            }
            if (c == '\\') {
                ++cursor;
                if (cursor == input.size()) fail("unexpected end when parsing string");
                const char escaped = input[cursor++];
                switch (escaped) {
                case '/': case '"': case '\\': result += escaped; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u': {
                    auto scalar = readHex();
                    if (scalar >= 0xD800 && scalar <= 0xDFFF) {
                        if (scalar >= 0xDC00) fail("unexpected character when parsing string");
                        if (cursor == input.size()) fail("unexpected end when parsing string");
                        if (input[cursor++] != '\\') fail("unexpected character when parsing string");
                        if (cursor == input.size()) fail("unexpected end when parsing string");
                        if (input[cursor++] != 'u') fail("unexpected character when parsing string");
                        const auto trailing = readHex();
                        if (trailing < 0xDC00 || trailing > 0xDFFF) fail("invalid UTF-16 trail surrogate");
                        scalar = 0x10000 + ((scalar & 0x3FF) << 10) + (trailing & 0x3FF);
                    }
                    if (!(scalar < 0xD800 || (scalar > 0xDFFF && scalar <= 0x10FFFF)))
                        fail("invalid UTF value");
                    if (scalar < 0x80) result += static_cast<char>(scalar);
                    else if (scalar < 0x800) {
                        result += static_cast<char>(0xC0 | (scalar >> 6));
                        result += static_cast<char>(0x80 | (scalar & 0x3F));
                    } else if (scalar < 0x10000) {
                        result += static_cast<char>(0xE0 | (scalar >> 12));
                        result += static_cast<char>(0x80 | ((scalar >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (scalar & 0x3F));
                    } else {
                        result += static_cast<char>(0xF0 | (scalar >> 18));
                        result += static_cast<char>(0x80 | ((scalar >> 12) & 0x3F));
                        result += static_cast<char>(0x80 | ((scalar >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (scalar & 0x3F));
                    }
                    break;
                }
                default: fail("unexpected character when parsing string");
                }
            } else {
                if (c < 0x20) fail("unexpected character when parsing string");
                result += input[cursor++];
            }
        }
    };
    const auto attach = [&](std::shared_ptr<Node> node) {
        if (stack.empty()) root = std::move(node);
        else if (stack.back().node->kind == JsonKind::Array)
            stack.back().node->elements.push_back(std::move(node));
        else {
            auto& frame = stack.back();
            frame.node->members.emplace_back(std::move(frame.key), std::move(node));
            frame.key.clear();
        }
    };
    const auto complete = [&](std::uint64_t bytes) {
        if (stack.empty()) done = true;
        else { stack.back().contentBytes += bytes; step = Step::AfterValue; }
    };
    const auto close = [&] {
        const auto bytes = stack.back().contentBytes;
        if (bytes > std::numeric_limits<std::uint32_t>::max())
            fail("object or array serialized size is limited to 2^32-1");
        stack.pop_back();
        complete(bytes + 5);
    };
    while (!done) {
        if (step == Step::ObjectFirst || step == Step::ArrayFirst) {
            const bool object = step == Step::ObjectFirst;
            skipSpaces();
            if (cursor == input.size()) fail(object
                ? "unexpected end of input data after '{'" : "unexpected end of input data after '['");
            if (input[cursor] == (object ? '}' : ']')) { ++cursor; close(); }
            else step = object ? Step::Key : Step::Value;
            continue;
        }
        if (step == Step::Key) {
            skipSpaces();
            if (cursor == input.size()) fail("unexpected end of object key");
            if (input[cursor] != '"') fail("expected '\"' when start parsing object key");
            auto key = readString(true);
            skipSpaces();
            if (cursor == input.size()) fail("unexpected end after object key");
            if (input[cursor] != ':') fail("unexpected character after key");
            ++cursor;
            stack.back().contentBytes += key.size() + 1;
            stack.back().key = std::move(key);
            step = Step::Value;
            continue;
        }
        if (step == Step::AfterValue) {
            const bool object = stack.back().node->kind == JsonKind::Object;
            skipSpaces();
            if (cursor == input.size()) fail(object
                ? "unexpected end when parsing object" : "unexpected end when parsing array");
            const char c = input[cursor++];
            if (c == ',') step = object ? Step::Key : Step::Value;
            else if (c == (object ? '}' : ']')) close();
            else fail(object ? "expected ',' or `}` when parsing object" : "expected ',' or `]` when parsing array");
            continue;
        }
        skipSpaces();
        if (cursor == input.size()) fail("unexpected end when start parsing JSON value");
        auto node = std::make_shared<Node>();
        const char c = input[cursor];
        if (c == '{' || c == '[') {
            ++cursor;
            node->kind = c == '{' ? JsonKind::Object : JsonKind::Array;
            attach(node);
            stack.push_back(Frame{std::move(node)});
            step = c == '{' ? Step::ObjectFirst : Step::ArrayFirst;
        } else if (c == '"') {
            node->kind = JsonKind::String;
            node->bytes = readString(false);
            const auto bytes = node->bytes.size() + std::uint64_t{5};
            attach(std::move(node)); complete(bytes);
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            const auto start = cursor++;
            while (cursor < input.size()) {
                const char digit = input[cursor];
                if (!(digit == '+' || digit == '-' || digit == '.' || digit == 'e' || digit == 'E' ||
                        (digit >= '0' && digit <= '9'))) break;
                ++cursor;
            }
            const auto length = cursor - start;
            if (length > 255) fail("number length is limited to 255 characters");
            node->kind = JsonKind::Number; node->bytes = input.substr(start, length);
            attach(std::move(node)); complete(length + std::uint64_t{2});
        } else if (c == 't' || c == 'f' || c == 'n') {
            const std::string_view word = c == 't' ? "true" : c == 'f' ? "false" : "null";
            const auto message = [&](bool end) {
                return std::string(end ? "unexpected end when parsing '" : "unexpected character when parsing '") +
                    std::string(word) + "'";
            };
            if (input.size() - cursor >= word.size()) {
                // The original x64 fast path diagnoses a mismatch at the start.
                if (input.substr(cursor, word.size()) != word) fail(message(false));
                cursor += word.size();
            } else {
                ++cursor;
                for (std::size_t i = 1; i < word.size(); ++i) {
                    if (cursor == input.size()) fail(message(true));
                    if (input[cursor] != word[i]) fail(message(false));
                    ++cursor;
                }
            }
            node->kind = c == 'n' ? JsonKind::Null : JsonKind::Boolean;
            node->boolean = c == 't'; attach(std::move(node)); complete(1);
        } else fail("unexpected character when start parsing JSON value");
    }
    return TypedJsonDocument(TypedJsonView(std::move(root)));
}
JsonKind TypedJsonView::kind() const { if (!node_) detail::throwMissingFghjValue(); return node_->kind; }
TypedJsonView TypedJsonView::findFirst(std::string_view key) const {
    if (!node_ || node_->kind != JsonKind::Object) return {};
    for (const auto& [name, value] : node_->members) if (name == key) return TypedJsonView(value);
    return {};
}
TypedJsonElementRange TypedJsonView::byElement() const noexcept {
    if (!node_ || node_->kind != JsonKind::Array) return {};
    return TypedJsonElementRange(node_);
}

bool TypedJsonElementRange::empty() const noexcept {
    return !node_ || index_ >= node_->elements.size();
}

TypedJsonView TypedJsonElementRange::front() const {
    if (empty()) {
#ifndef NDEBUG
        throw d_runtime::ngStaticError<d_runtime::AssertError>(
            d_runtime::RetainedString::fromStaticLiteral("Assertion failure"),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_SOURCE_FILE), 463);
#else
        return {};
#endif
    }
    return TypedJsonView(node_->elements[index_]);
}

void TypedJsonElementRange::popFront() noexcept {
    if (!empty()) ++index_;
}

TypedJsonElementRange::Iterator TypedJsonElementRange::begin() const noexcept {
    return Iterator(node_, index_);
}

TypedJsonElementRange::Iterator TypedJsonElementRange::end() const noexcept {
    return Iterator(node_, node_ ? node_->elements.size() : 0);
}

TypedJsonView TypedJsonElementRange::Iterator::operator*() const {
    return TypedJsonElementRange(node_, index_).front();
}

TypedJsonElementRange::Iterator& TypedJsonElementRange::Iterator::operator++() noexcept {
    if (node_ && index_ < node_->elements.size()) ++index_;
    return *this;
}

std::vector<TypedJsonView> TypedJsonView::elements() const {
    std::vector<TypedJsonView> result;
    // Fghj.byElement returns an empty range for missing/non-array values.
    if (!node_ || node_->kind != JsonKind::Array) return result;
    result.reserve(node_->elements.size()); for (const auto& value : node_->elements) result.emplace_back(value);
    return result;
}
std::span<const std::shared_ptr<detail::TypedJsonNode>> TypedJsonView::elementNodes() const noexcept {
    return node_->elements;
}
TypedJsonMemberRange TypedJsonView::byKeyValue() const noexcept {
    if (!node_ || node_->kind != JsonKind::Object) return {};
    return TypedJsonMemberRange(node_);
}

bool TypedJsonMemberRange::empty() const noexcept {
    return !node_ || index_ >= node_->members.size();
}

TypedJsonMemberRange::Value TypedJsonMemberRange::front() const {
    if (empty()) {
#ifndef NDEBUG
        throw d_runtime::ngStaticError<d_runtime::AssertError>(
            d_runtime::RetainedString::fromStaticLiteral("Assertion failure"),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_SOURCE_FILE), 559);
#else
        return {};
#endif
    }
    const auto& entry = node_->members[index_];
    return {d_runtime::RetainedString::retainedView(node_, entry.first.data(), entry.first.size()),
        TypedJsonView(entry.second)};
}

void TypedJsonMemberRange::popFront() noexcept {
    if (!empty()) ++index_;
}

TypedJsonMemberRange::Iterator TypedJsonMemberRange::begin() const noexcept {
    return Iterator(*this);
}

TypedJsonMemberRange::Iterator TypedJsonMemberRange::end() const noexcept {
    return Iterator(TypedJsonMemberRange(node_, node_ ? node_->members.size() : 0));
}

std::vector<std::pair<std::string, TypedJsonView>> TypedJsonView::members() const {
    std::vector<std::pair<std::string, TypedJsonView>> result;
    // Fghj.byKeyValue returns an empty range for missing/non-object values.
    if (!node_ || node_->kind != JsonKind::Object) return result;
    result.reserve(node_->members.size()); for (const auto& [name, value] : node_->members) result.emplace_back(name, TypedJsonView(value));
    return result;
}
std::string_view TypedJsonView::numberToken() const { requireKind(kind(), JsonKind::Number); return node_->bytes; }
std::string_view TypedJsonView::stringBytes() const { requireKind(kind(), JsonKind::String); return node_->bytes; }
bool TypedJsonView::boolean() const { requireKind(kind(), JsonKind::Boolean); return node_->boolean; }
namespace {
ReadError unexpectedSourceKind(unsigned kind) {
    // FGHJ serialization.d335-347: one immutable exception per enum member.
    // These genuine exception owners are shared, not thread-local/staticError.
    static const std::shared_ptr<source_exceptions::SerdeException> errors[] = {
        std::make_shared<source_exceptions::SerdeException>(
            d_runtime::RetainedString::fromStaticLiteral("Unexpected FGHJ kind null_"),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_SERIALIZATION_SOURCE_FILE), 337),
        std::make_shared<source_exceptions::SerdeException>(
            d_runtime::RetainedString::fromStaticLiteral("Unexpected FGHJ kind false_"),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_SERIALIZATION_SOURCE_FILE), 337),
        std::make_shared<source_exceptions::SerdeException>(
            d_runtime::RetainedString::fromStaticLiteral("Unexpected FGHJ kind true_"),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_SERIALIZATION_SOURCE_FILE), 337),
        std::make_shared<source_exceptions::SerdeException>(
            d_runtime::RetainedString::fromStaticLiteral("Unexpected FGHJ kind number"),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_SERIALIZATION_SOURCE_FILE), 337),
        std::make_shared<source_exceptions::SerdeException>(
            d_runtime::RetainedString::fromStaticLiteral("Unexpected FGHJ kind string"),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_SERIALIZATION_SOURCE_FILE), 337),
        std::make_shared<source_exceptions::SerdeException>(
            d_runtime::RetainedString::fromStaticLiteral("Unexpected FGHJ kind array"),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_SERIALIZATION_SOURCE_FILE), 337),
        std::make_shared<source_exceptions::SerdeException>(
            d_runtime::RetainedString::fromStaticLiteral("Unexpected FGHJ kind object"),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_SERIALIZATION_SOURCE_FILE), 337),
        std::make_shared<source_exceptions::SerdeException>(
            d_runtime::RetainedString::fromStaticLiteral("Wrong encoding of FGHJ kind"),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_SERIALIZATION_SOURCE_FILE), 346)
    };
    return errors[kind < 7 ? kind : 7];
}
}

ReadError unexpectedJsonKind(JsonKind kind) {
    switch (kind) {
    case JsonKind::Null: return unexpectedSourceKind(0);
    case JsonKind::Boolean: return unexpectedSourceKind(1);
    case JsonKind::Number: return unexpectedSourceKind(3);
    case JsonKind::String: return unexpectedSourceKind(4);
    case JsonKind::Array: return unexpectedSourceKind(5);
    case JsonKind::Object: return unexpectedSourceKind(6);
    }
    return unexpectedSourceKind(7);
}
ReadError unexpectedJsonKind(TypedJsonView data) {
    if (data.kind() == JsonKind::Boolean) return unexpectedSourceKind(data.boolean() ? 2 : 1);
    return unexpectedJsonKind(data.kind());
}

ReadError deserializeValue(TypedJsonView data, bool& value) {
    if (data.kind() != JsonKind::Boolean) return unexpectedJsonKind(data);
    value = data.boolean(); return {};
}
ReadError deserializeValue(TypedJsonView data, std::optional<std::string>& value) {
    if (data.kind() == JsonKind::Null) { value.reset(); return {}; }
    if (data.kind() != JsonKind::String) return unexpectedJsonKind(data);
    if (data.stringBytes().empty()) value.reset(); else value = std::string(data.stringBytes());
    return {};
}
ReadError deserializeValue(TypedJsonView data, std::string& value) {
    if (data.kind() == JsonKind::Null) { value.clear(); return {}; }
    if (data.kind() != JsonKind::String) return unexpectedJsonKind(data);
    value = data.stringBytes(); return {};
}
ReadError deserializeValue(TypedJsonView data, d_runtime::RetainedString& value) {
    const auto kind = data.kind();
    if (kind == JsonKind::Null) { value.reset(); return {}; }
    if (kind != JsonKind::String) return unexpectedJsonKind(data);
    const auto bytes = data.stringBytes();
    if (bytes.empty()) { value.reset(); return {}; }
    try {
        // This is ordinary deserializeValue, not the borrowing scoped-string API.
        auto duplicate = d_runtime::RetainedString(bytes);
        value = std::move(duplicate);
    } catch (const std::bad_alloc&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(
            d_runtime::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    } catch (const std::length_error&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(
            d_runtime::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    }
    return {};
}
ReadError deserializeValue(TypedJsonView data, float& value) { return readFloating(data, value, false); }
ReadError deserializeValue(TypedJsonView data, double& value) { return readFloating(data, value, false); }
namespace {
ReadError unexpectedFloat2Kind(TypedJsonView data) {
    // The Source unexpectedKind message template has its own static objects.
    static const std::shared_ptr<source_exceptions::SerdeException> errors[] = {
        std::make_shared<source_exceptions::SerdeException>(
            d_runtime::RetainedString::fromStaticLiteral("Failed to deserialize value of float[2] null_"),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_SERIALIZATION_SOURCE_FILE), 337),
        std::make_shared<source_exceptions::SerdeException>(
            d_runtime::RetainedString::fromStaticLiteral("Failed to deserialize value of float[2] false_"),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_SERIALIZATION_SOURCE_FILE), 337),
        std::make_shared<source_exceptions::SerdeException>(
            d_runtime::RetainedString::fromStaticLiteral("Failed to deserialize value of float[2] true_"),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_SERIALIZATION_SOURCE_FILE), 337),
        std::make_shared<source_exceptions::SerdeException>(
            d_runtime::RetainedString::fromStaticLiteral("Failed to deserialize value of float[2] number"),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_SERIALIZATION_SOURCE_FILE), 337),
        std::make_shared<source_exceptions::SerdeException>(
            d_runtime::RetainedString::fromStaticLiteral("Failed to deserialize value of float[2] string"),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_SERIALIZATION_SOURCE_FILE), 337),
        std::make_shared<source_exceptions::SerdeException>(
            d_runtime::RetainedString::fromStaticLiteral("Failed to deserialize value of float[2] array"),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_SERIALIZATION_SOURCE_FILE), 337),
        std::make_shared<source_exceptions::SerdeException>(
            d_runtime::RetainedString::fromStaticLiteral("Failed to deserialize value of float[2] object"),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_SERIALIZATION_SOURCE_FILE), 337),
        std::make_shared<source_exceptions::SerdeException>(
            d_runtime::RetainedString::fromStaticLiteral("Wrong encoding of FGHJ kind"),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_SERIALIZATION_SOURCE_FILE), 346)
    };
    switch (data.kind()) {
    case JsonKind::Null: return errors[0];
    case JsonKind::Boolean: return errors[data.boolean() ? 2 : 1];
    case JsonKind::Number: return errors[3];
    case JsonKind::String: return errors[4];
    case JsonKind::Array: return errors[5];
    case JsonKind::Object: return errors[6];
    }
    return errors[7];
}
}

ReadError deserializeValue(TypedJsonView data, std::array<float, 2>& value) {
    const auto kind = data.kind();
    if (kind == JsonKind::Null) return {};
    if (kind != JsonKind::Array) return unexpectedFloat2Kind(data);
    // Keep the actual immutable graph owner and walk its existing element range;
    // no allocating elements() vector or substitute JSON graph is introduced.
    const auto& entries = data.node_->elements;
    for (std::size_t i = 0; i < value.size() && i < entries.size(); ++i)
        if (auto error = deserializeValue(TypedJsonView(entries[i]), value[i])) return error;
    return {};
}

ReadError deserializeNumber(TypedJsonView data, float& value) { return readFloating(data, value, true); }
ReadError deserializeNumber(TypedJsonView data, double& value) { return readFloating(data, value, true); }
} // namespace nicxlive::core::serde
