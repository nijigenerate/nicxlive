#pragma once

// Phobos std.utf.UTFException and druntime core.exception.UnicodeException.
// Copyright The D Language Foundation 2000-2012; Sean Kelly 2005-2013.
// BSL-1.0; see licenses/phobos-unicode.txt.
#include "throwable.hpp"
#include "assert_error.hpp"
#include "array_range_error.hpp"
#include "system_error.hpp"
#include "static_error.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <source_location>
#include <span>
#include <string>
#include <stdexcept>
#include <utility>

namespace nicxlive::core::d_runtime {

class UnicodeException : public ThrowableException {
public:
    std::size_t idx;
    UnicodeException(RetainedString message, std::size_t index,
        std::shared_ptr<Throwable> tail = {}, std::source_location source = std::source_location::current())
        : ThrowableException(std::move(message), std::move(tail), source), idx(index) {}
    UnicodeException(RetainedString message, std::size_t index, RetainedString sourceFile,
        std::size_t sourceLine, std::shared_ptr<Throwable> tail = {})
        : ThrowableException(std::move(message), std::move(sourceFile), sourceLine, std::move(tail)), idx(index) {}
    std::string_view ngThrowableTypeName() const noexcept override { return "core.exception.UnicodeException"; }
};

class UTFException : public UnicodeException {
public:
    std::array<std::uint32_t, 4> sequence{};
    std::size_t len = 0;
    UTFException(RetainedString message, std::shared_ptr<Throwable> tail = {},
        std::source_location source = std::source_location::current())
        : UnicodeException(std::move(message), 0, std::move(tail), source) {}
    UTFException(RetainedString message, std::size_t index, std::shared_ptr<Throwable> tail = {},
        std::source_location source = std::source_location::current())
        : UnicodeException(indexedMessage(message, index), index, std::move(tail), source) {}
    UTFException(RetainedString message, RetainedString sourceFile, std::size_t sourceLine,
        std::shared_ptr<Throwable> tail = {})
        : UnicodeException(std::move(message), 0, std::move(sourceFile), sourceLine, std::move(tail)) {}
    UTFException(RetainedString message, std::size_t index, RetainedString sourceFile,
        std::size_t sourceLine, std::shared_ptr<Throwable> tail = {})
        : UnicodeException(indexedMessage(message, index), index,
            std::move(sourceFile), sourceLine, std::move(tail)) {}
    UTFException& setSequence(std::span<const std::uint32_t> data) {
#ifndef NDEBUG
        if (data.size() > 4) throw ngStaticError<AssertError>(
            RetainedString::fromStaticLiteral("Assertion failure"),
            RetainedString::fromStaticLiteral("std/utf.d"), 84);
#endif
        len = std::min<std::size_t>(data.size(), 4);
        std::copy_n(data.begin(), len, sequence.begin());
        return *this;
    }
    std::string_view ngThrowableTypeName() const noexcept override { return "std.utf.UTFException"; }
    using UnicodeException::toString;
    RetainedString toString() const override {
        if (len == 0) return UnicodeException::toString();
        const auto count = len;
        if (count > sequence.size()) throw ngStaticError<ArraySliceError>(0, count, sequence.size(),
            RetainedString::fromStaticLiteral("std/utf.d"), 127);
        const auto values = std::span<const std::uint32_t>(sequence.data(), count);
        auto result = allocateUtfText([] { return std::string("Invalid UTF sequence:"); });
        for (const auto value : values) {
            // The source uses a lowercase hexadecimal value, leading zero
            // only for one-digit values, and an 'x' suffix, not a prefix.
            char digits[8];
            allocateUtfText([&] { result += ' '; });
            const auto end = std::to_chars(digits, digits + sizeof(digits), value, 16).ptr;
            allocateUtfText([&] {
                if (end == digits + 1) result += '0';
                result.append(digits, end);
                result += 'x';
            });
        }
        if (!msg.empty()) {
            const auto message = msg.view();
            allocateUtfText([&] { result += " - "; result.append(message); });
        }
        return allocateUtfText([&] { return RetainedString(std::move(result)); });
    }
private:
    // Callers pass only passive byte/backing construction, never decoding or virtual calls.
    template<class Build> static auto allocateUtfText(Build&& build) -> decltype(std::forward<Build>(build)()) {
        try { return std::forward<Build>(build)(); }
        catch (const std::bad_alloc&) {
            throw ngStaticError<OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw ngStaticError<OutOfMemoryError>(std::source_location::current());
        }
    }

    static RetainedString indexedMessage(const RetainedString& message, std::size_t index) {
        char digits[3 * sizeof(std::size_t)];
        const auto end = std::to_chars(digits, digits + sizeof(digits), index).ptr;
        return allocateUtfText([&] {
            std::string result(message.view());
            result += " (at index ";
            result.append(digits, end);
            result += ')';
            return RetainedString(std::move(result));
        });
    }
};

} // namespace nicxlive::core::d_runtime
