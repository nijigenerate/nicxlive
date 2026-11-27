#pragma once
// Current Phobos std.file.FileException and Windows error-message closure.
// Copyright The D Language Foundation 2006-2013. BSL-1.0;
// see licenses/phobos-psd-file.txt. Native exception ABI remains separate.
#include "phobos_unicode_cursor.hpp"
#include "throwable.hpp"
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <source_location>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace nicxlive::core::d_runtime {
namespace file_exception_detail {
inline std::uint32_t currentError() noexcept {
#ifdef _WIN32
    return GetLastError();
#else
    return static_cast<std::uint32_t>(errno);
#endif
}

#ifdef _WIN32
inline bool white(char32_t value) noexcept {
    return (value >= 9 && value <= 13) || value == 32 || value == 0x85 || value == 0xa0 ||
        value == 0x1680 || (value >= 0x2000 && value <= 0x200a) || value == 0x2028 ||
        value == 0x2029 || value == 0x202f || value == 0x205f || value == 0x3000;
}

inline void appendScalar(std::string& output, char32_t scalar) {
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
}
#endif

#ifndef _WIN32
template<class Result> RetainedString errnoMessage(Result result, const char* buffer) {
    if constexpr (std::is_pointer_v<Result>) {
        return RetainedString(std::string_view(result));
    } else {
        if (result != 0) return RetainedString::fromStaticLiteral("Unknown error");
        return RetainedString(std::string_view(buffer));
    }
}
#endif

inline RetainedString systemMessage(std::uint32_t code) {
#ifdef _WIN32
    static_assert(sizeof(wchar_t) == 2);
    std::string result;
    bool success = false;
    try {
        wchar_t* buffer = nullptr;
        const auto count = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
        struct Release {
            wchar_t* buffer;
            ~Release() { if (buffer) LocalFree(buffer); }
        } release{buffer};
        if (buffer) {
            using Cursor = phobos::detail::UnicodeCursor<std::span<const wchar_t>, wchar_t>;
            std::size_t first = 0, last = count;
            Cursor left(std::span<const wchar_t>{buffer, last});
            while (!left.empty() && white(left.front())) {
                left.popFront();
                ++first;
            }
            Cursor right(std::span<const wchar_t>{buffer + first, last - first});
            while (!right.empty() && white(right.back())) {
                right.popBack();
                --last;
            }
            Cursor values(std::span<const wchar_t>{buffer + first, last - first});
            while (!values.empty()) {
                appendScalar(result, values.front());
                values.popFront();
            }
            success = true;
        }
    } catch (const ThrowableException&) {
        // writeErrorMessage discards a partial conversion before its fallback.
        result.clear();
    }
    if (!success) {
        result.append("Error ");
        std::array<char, 10> digits;
        const auto converted = std::to_chars(digits.data(), digits.data() + digits.size(), code);
        result.append(digits.data(), static_cast<std::size_t>(converted.ptr - digits.data()));
    }
    return result.empty() ? RetainedString{} : RetainedString(std::move(result));
#else
    std::array<char, 1024> buffer;
    const auto result = ::strerror_r(static_cast<int>(code), buffer.data(), buffer.size());
    return errnoMessage(result, buffer.data());
#endif
}

inline RetainedString message(RetainedString name, const RetainedString& detail) {
    if (detail.empty()) {
        if (name.isNull()) return RetainedString::fromStaticLiteral("(null)");
        return name.empty() ? RetainedString{} : RetainedString(name.view());
    }
    std::string result;
    if (name.isNull()) result.append("(null)");
    else if (!name.empty()) result.append(name.data(), name.size());
    result.append(": ");
    result.append(detail.data(), detail.size());
    return RetainedString(std::move(result));
}
} // namespace file_exception_detail

class FileException : public ThrowableException {
public:
    // Source field errno; the native spelling avoids the platform errno macro.
    const std::uint32_t errorNumber;
    FileException(RetainedString name, RetainedString detail,
        std::source_location source = std::source_location::current())
        : FileException(std::move(name), std::move(detail), 0, source) {}
    explicit FileException(RetainedString name,
        std::uint32_t code = file_exception_detail::currentError(),
        std::source_location source = std::source_location::current())
        : FileException(std::move(name), file_exception_detail::systemMessage(code), code, source) {}
    std::string_view ngThrowableTypeName() const noexcept override { return "std.file.FileException"; }
private:
    FileException(RetainedString name, RetainedString detail, std::uint32_t code, std::source_location source)
        : ThrowableException(file_exception_detail::message(std::move(name), detail), {}, source), errorNumber(code) {}
};
} // namespace nicxlive::core::d_runtime
