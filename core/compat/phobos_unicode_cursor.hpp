#pragma once

// Source mapping of Phobos std.range.primitives narrow-string operations and
// std.utf decoding. Copyright The D Language Foundation 2000-2012. BSL-1.0;
// see licenses/phobos-unicode.txt. Native exception ABI remains separate.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include "phobos_unicode_exception.hpp"

namespace nicxlive::core::d_runtime::phobos::detail {

// Retain the actual descriptor, never snapshot or eagerly decode its bytes.
// UTF-32 arrays use IndexedCursor: their fronts remain individual references.
template<class R, class Unit> class UnicodeCursor {
public:
    using value_type = char32_t;
    static constexpr bool infinite = false;
    explicit UnicodeCursor(R input) : input_(std::move(input)), last_(input_.size()) {}
    bool empty() const { return first_ == last_; }
    char32_t front() const { requireElement(); return decode(first_); }
    char32_t back() const { requireElement(); return decode(last_ - backWidth()); }
    void popFront() {
        requireElement();
        const auto head = unit(first_);
        std::size_t width;
        if constexpr (sizeof(Unit) == 1) {
            // Source popFront does not validate, including obsolete 5/6-byte
            // widths and the clamp when the remaining input is truncated.
            width = head < 192 ? 1 : head < 224 ? 2 : head < 240 ? 3
                : head < 248 ? 4 : head < 252 ? 5 : head < 254 ? 6 : 1;
        } else {
            width = 1 + (head >= 0xd800 && head <= 0xdbff);
        }
        first_ += std::min(width, last_ - first_);
    }
    void popBack() { requireElement(); last_ -= backWidth(); }
    auto save() const { return *this; }

private:
    std::uint32_t unit(std::size_t index) const {
        using Unsigned = std::make_unsigned_t<std::remove_cv_t<Unit>>;
        if constexpr (requires { input_[index]; })
            return static_cast<Unsigned>(input_[index]);
        else return static_cast<Unsigned>(input_.data()[index]);
    }
    void requireElement() const {
        if (empty()) throw std::out_of_range("Empty Unicode range");
    }
    std::size_t backWidth() const {
        if constexpr (sizeof(Unit) == 1) {
            if ((unit(last_ - 1) & 0xc0) != 0x80) return 1;
            for (std::size_t width = 2; width <= 4 && width <= last_ - first_; ++width)
                if ((unit(last_ - width) & 0xc0) != 0x80) return width;
            throw UTFException(RetainedString::fromStaticLiteral("Not the end of the UTF sequence"), last_ - first_);
        } else {
            const auto tail = unit(last_ - 1);
            const std::size_t width = 1 + (tail >= 0xdc00 && tail < 0xe000);
            // Source popBack slices with strideBack without decoding. A lone
            // final low surrogate fails before the descriptor is changed.
            if (width > last_ - first_) throw std::out_of_range("Unicode slice bounds");
            return width;
        }
    }
    char32_t decode(std::size_t offset) const {
        auto scalar = unit(offset);
        if constexpr (sizeof(Unit) == 1) {
            if (scalar < 0x80) return static_cast<char32_t>(scalar);
            if ((scalar & 0xc0) != 0xc0) invalidUtf8(offset);
            auto marker = static_cast<std::uint8_t>(scalar << 1);
            constexpr std::uint32_t masks[] = {0x7f, 0x7ff, 0xffff, 0x1fffff};
            for (std::size_t i = 1; i <= 3; ++i) {
                if (i == last_ - offset)
                    invalidUtf8(offset, true);
                const auto next = unit(offset + i);
                if ((next & 0xc0) != 0x80) invalidUtf8(offset);
                scalar = (scalar << 6) | (next & 0x3f);
                marker = static_cast<std::uint8_t>(marker << 1);
                if (!(marker & 0x80)) {
                    scalar &= masks[i];
                    if ((scalar & ~masks[i - 1]) == 0
                        || (i == 2 && scalar >= 0xd800 && scalar <= 0xdfff)
                        || (i == 3 && scalar > 0x10ffff)) invalidUtf8(offset);
                    return static_cast<char32_t>(scalar);
                }
            }
            invalidUtf8(offset);
        } else {
            if (scalar >= 0xd800 && scalar <= 0xdbff) {
                if (last_ - offset == 1)
                    invalidUtf16(offset, "surrogate UTF-16 high value past end of string");
                const auto low = unit(offset + 1);
                if (low < 0xdc00 || low > 0xdfff)
                    invalidUtf16(offset, "surrogate UTF-16 low value out of range");
                scalar = ((scalar - 0xd7c0) << 10) + low - 0xdc00;
            } else if (scalar >= 0xdc00 && scalar <= 0xdfff) {
                invalidUtf16(offset, "unpaired surrogate UTF-16 value");
            }
            return static_cast<char32_t>(scalar);
        }
    }
    [[noreturn]] void invalidUtf8(std::size_t offset, bool truncated = false) const {
        std::array<std::uint32_t, 4> sequence{};
        std::size_t count = 0;
        do {
            sequence[count] = unit(offset + count);
        } while (++count < last_ - offset && count < 4 && (unit(offset + count) & 0xc0) == 0x80);
        // Phobos reports this copied sequence length, not the original buffer
        // offset or the number of code units already consumed by the decoder.
        UTFException error(truncated?RetainedString::fromStaticLiteral("Attempted to decode past the end of a string")
            :RetainedString::fromStaticLiteral("Invalid UTF-8 sequence"),count,
            RetainedString::fromStaticLiteral("std/utf.d"),1556);
        error.setSequence(std::span<const std::uint32_t>(sequence.data(), count));
        throw error;
    }
    template<std::size_t N> [[noreturn]] void invalidUtf16(std::size_t offset, const char (&message)[N]) const {
        const std::uint32_t first = unit(offset);
        UTFException error(RetainedString::fromStaticLiteral(message),
            RetainedString::fromStaticLiteral("std/utf.d"),1765);
        error.setSequence(std::span<const std::uint32_t>(&first, 1));
        throw error;
    }
    R input_;
    std::size_t first_ = 0, last_ = 0;
};

} // namespace nicxlive::core::d_runtime::phobos::detail
