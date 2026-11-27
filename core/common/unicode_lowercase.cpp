#include "unicode_lowercase.hpp"
#include "detail/phobos_lowercase_data.hpp"
#include "../compat/system_error.hpp"
#include "../compat/static_error.hpp"
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nicxlive::core::common::unicode {
namespace {
// Conversion, table lookup and authored range errors stay outside this boundary.
template<class Build> auto allocateCaseBytes(Build&& build) {
    try { return std::forward<Build>(build)(); }
    catch (const std::bad_alloc&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
    }
}

std::uint16_t lowerIndex(std::uint32_t scalar) noexcept {
    namespace data = detail::phobos_lowercase_data;
    const auto packed = [](std::uint64_t offset, std::size_t index, unsigned bits) {
        const auto word = data::trieWords[offset + index / (64u / bits)];
        return static_cast<std::uint32_t>((word >> (bits * (index % (64u / bits)))) & ((1ULL << bits) - 1));
    };
    auto index = packed(data::trieOffsets[0], scalar >> 13, 8);
    index = packed(data::trieOffsets[1], (index << 7) + ((scalar >> 6) & 0x7f), 16);
    return static_cast<std::uint16_t>(packed(data::trieOffsets[2], (index << 6) + (scalar & 0x3f), 16));
}
std::size_t encodedLength(std::uint32_t scalar) noexcept {
    return scalar < 0x80 ? 1 : scalar < 0x800 ? 2 : scalar < 0x10000 ? 3 : 4;
}
void appendUtf8(std::string& output, std::uint32_t scalar) {
    allocateCaseBytes([&] {
        if (scalar < 0x80) output.push_back(static_cast<char>(scalar));
        else if (scalar < 0x800) {
            output.push_back(static_cast<char>(0xc0 | (scalar >> 6)));
            output.push_back(static_cast<char>(0x80 | (scalar & 0x3f)));
        } else if (scalar < 0x10000) {
            output.push_back(static_cast<char>(0xe0 | (scalar >> 12)));
            output.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (scalar & 0x3f)));
        } else {
            output.push_back(static_cast<char>(0xf0 | (scalar >> 18)));
            output.push_back(static_cast<char>(0x80 | ((scalar >> 12) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (scalar & 0x3f)));
        }
    });
}
} // namespace

std::optional<std::size_t> lowercasePrefix(std::string_view text) {
    namespace data = detail::phobos_lowercase_data;
    ReplacementUtf8Cursor cursor{text};
    std::size_t prefix = 0;
    bool changes = false;
    while (!cursor.empty()) {
        const auto scalar = cursor.take();
        if (lowerIndex(scalar) != 0xffff) { changes = true; break; }
        prefix += encodedLength(scalar);
    }
    if (!changes) return std::nullopt;
    return prefix;
}

std::string lowercaseChanged(std::string_view text, std::size_t prefix) {
    namespace data = detail::phobos_lowercase_data;
    std::string result;
    allocateCaseBytes([&] { result.reserve(text.size()); });
    // Original toCase counts re-encoded scalar lengths, not consumed input bytes.
    if (prefix > text.size()) throw CaseBoundsError{prefix, text.size()};
    allocateCaseBytes([&] { result.append(text.data(), prefix); });
    ReplacementUtf8Cursor cursor{text, prefix};
    while (!cursor.empty()) {
        auto scalar = cursor.take();
        if (scalar < 0x80) {
            if (scalar >= 'A' && scalar <= 'Z') scalar += 'a' - 'A';
            appendUtf8(result, scalar);
            continue;
        }
        const auto index = lowerIndex(scalar);
        if (index == 0xffff) appendUtf8(result, scalar);
        else if (index < data::simpleLowerLimit) appendUtf8(result, data::lowerValues[index]);
        else {
            const auto entry = data::lowerValues[index];
            appendUtf8(result, entry & 0xffffff);
            for (std::uint32_t i = 1; i < (entry >> 24); ++i) appendUtf8(result, data::lowerValues[index + i]);
        }
    }
    return result;
}

std::string toLowerBytes(std::string_view text) {
    const auto prefix = lowercasePrefix(text);
    if (prefix) return lowercaseChanged(text, *prefix);
    return allocateCaseBytes([&] { return std::string(text); });
}
NullableString toLower(NullableString text) {
    const auto prefix = lowercasePrefix(text.view());
    if (!prefix) return text;
    auto changed = lowercaseChanged(text.view(), *prefix);
    return allocateCaseBytes([&] { return NullableString(std::move(changed)); });
}
d_runtime::RetainedString toLower(d_runtime::RetainedString text) {
    const auto prefix = lowercasePrefix(text.view());
    if (!prefix) return text;
    auto changed = lowercaseChanged(text.view(), *prefix);
    return allocateCaseBytes([&] { return d_runtime::RetainedString(std::move(changed)); });
}
} // namespace nicxlive::core::common::unicode
