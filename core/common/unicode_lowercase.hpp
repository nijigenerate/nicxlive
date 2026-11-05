#pragma once
// Phobos std.uni/std.utf replacement decoding and full Unicode lowercase.
// Existing upstream authors and license: ../licenses/phobos-locale-case.txt.
#include "nullable_string.hpp"
#include "../compat/array_range_error.hpp"
#include "../compat/retained_string.hpp"
#include <cstdint>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>

namespace nicxlive::core::common::unicode {
struct ReplacementUtf8Cursor {
    std::string_view bytes;
    std::size_t offset{};
    bool empty() const { return offset == bytes.size(); }
    std::uint32_t take() {
        const auto start = offset;
        auto first = static_cast<std::uint8_t>(bytes[start]);
        if (first < 0x80) { ++offset; return first; }
        if ((first & 0xc0) != 0xc0) { ++offset; return 0xfffd; }
        std::uint32_t value = first;
        first = static_cast<std::uint8_t>(first << 1);
        constexpr std::uint32_t masks[]{0x7f, 0x7ff, 0xffff, 0x1fffff};
        for (std::size_t i = 1; i <= 3; ++i) {
            if (i == bytes.size() - start) { offset += i; return 0xfffd; }
            const auto next = static_cast<std::uint8_t>(bytes[start + i]);
            if ((next & 0xc0) != 0x80) { offset += i + 1; return 0xfffd; }
            value = (value << 6) | (next & 0x3f);
            first = static_cast<std::uint8_t>(first << 1);
            if (!(first & 0x80)) {
                value &= masks[i];
                offset += i + 1;
                if ((value & ~masks[i - 1]) == 0 || (i == 2 && value >= 0xd800 && value <= 0xdfff) ||
                    (i == 3 && value > 0x10ffff)) return 0xfffd;
                return value;
            }
        }
        offset += 4;
        return 0xfffd;
    }
};


class CaseBoundsError : public d_runtime::ArraySliceError {
public:
    const std::size_t offset;
    CaseBoundsError(std::size_t offset, std::size_t length,
        std::source_location source = std::source_location::current())
        : ArraySliceError(0, offset, length, source), offset(offset) {}
};

std::optional<std::size_t> lowercasePrefix(std::string_view text);
std::string lowercaseChanged(std::string_view text, std::size_t prefix);
std::string toLowerBytes(std::string_view text);
NullableString toLower(NullableString text);
d_runtime::RetainedString toLower(d_runtime::RetainedString text);
} // namespace nicxlive::core::common::unicode
