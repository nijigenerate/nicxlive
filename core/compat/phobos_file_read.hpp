#pragma once
// Phobos std.file.read/readImpl (std/file.d 317-481), BSL-1.0.
// Native payload allocation uses the canonical owning SharedArray lowering;
// this does not implement the general D GC allocator or its pressure behavior.
#include "phobos_file_exception.hpp"
#include "scope_exit.hpp"
#include "assert_error.hpp"
#include "system_error.hpp"
#include "../common/shared_array.hpp"
#include "../common/unicode_lowercase.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace nicxlive::core::d_runtime {
namespace file_read_detail {
template<class Operation> decltype(auto) allocateBytes(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    }
}
[[noreturn]] inline void fail(RetainedString name, std::uint32_t code, std::size_t line) {
    FileException error(std::move(name), code);
    error.file = RetainedString::fromStaticLiteral("std/file.d");
    error.line = line;
    throw error;
}
#ifdef _WIN32
inline std::wstring wide(RetainedString name) {
    std::wstring result;
    common::unicode::ReplacementUtf8Cursor cursor{name.view()};
    while (!cursor.empty()) {
        auto scalar = cursor.take();
        if (scalar <= 0xffff) allocateBytes([&] { result.push_back(static_cast<wchar_t>(scalar)); });
        else {
            scalar -= 0x10000;
            allocateBytes([&] { result.push_back(static_cast<wchar_t>(0xd800 + (scalar >> 10))); });
            allocateBytes([&] { result.push_back(static_cast<wchar_t>(0xdc00 + (scalar & 0x3ff))); });
        }
    }
    return result;
}
#endif
}
inline common::SharedArray<std::uint8_t> ngReadBinaryFile(RetainedString name,
        std::size_t upTo = std::numeric_limits<std::size_t>::max()) {
    std::vector<std::uint8_t> result;
#ifdef _WIN32
    const auto path = file_read_detail::wide(name);
    const auto handle = CreateFileW(name.isNull() ? nullptr : path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (handle == INVALID_HANDLE_VALUE) file_read_detail::fail(name, GetLastError(), 453);
    ngRunWithScopeExit([&] {
        DWORD high;
        const auto low = GetFileSize(handle, &high);
        if (low == INVALID_FILE_SIZE) file_read_detail::fail(name, GetLastError(), 456);
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(upTo,
            (std::uint64_t(high) << 32) | low));
        try {
            file_read_detail::allocateBytes([&] { result.resize(count); });
            for (std::size_t offset = 0; offset != count;) {
                const auto chunk = static_cast<DWORD>(std::min<std::size_t>(count - offset, 0xffff0000));
                DWORD actual;
                if (!ReadFile(handle, result.data() + offset, chunk, &actual, nullptr) || actual != chunk)
                    file_read_detail::fail(name, GetLastError(), 466);
                offset += actual;
            }
        } catch (...) {
            // Source scope(failure) frees the read payload before outer close.
            std::vector<std::uint8_t>{}.swap(result);
            throw;
        }
    }, [&] {
        if (!CloseHandle(handle)) file_read_detail::fail(name, GetLastError(), 454);
    });
#else
    // tempCString preserves source bytes (including embedded NUL) and null.
    const auto pathBytes = name.view();
    const auto path = file_read_detail::allocateBytes([&] { return std::string(pathBytes); });
    const auto fd = ::open(name.isNull() ? nullptr : path.c_str(), O_RDONLY);
    if (fd == -1) file_read_detail::fail(name, errno, 372);
    struct Close { int fd; ~Close() { ::close(fd); } } close{fd};
    struct stat status;
    if (::fstat(fd, &status) != 0) file_read_detail::fail(name, errno, 376);
    const auto maximum = std::numeric_limits<std::size_t>::max();
    const auto initial = status.st_size ? std::min<std::uintmax_t>(
        static_cast<std::uintmax_t>(status.st_size) + 1, maximum / 2) : 4096;
    file_read_detail::allocateBytes([&] {
        result.resize(std::min<std::size_t>(upTo, static_cast<std::size_t>(initial)));
    });
    std::size_t used = 0;
    for (;;) {
        const auto actual = ::read(fd, result.empty() ? nullptr : result.data() + used,
            std::min(result.size(), upTo) - used);
        if (actual == -1) file_read_detail::fail(name, errno, 390);
        if (actual == 0) break;
        used += static_cast<std::size_t>(actual);
        if (used >= upTo) break;
        if (used < result.size()) continue;
        // checked(size_t)+16384 uses Abort's unconditional source assertion.
        if (used > maximum - 16384)
            ngAssertUnreachable("Overflow on binary operator +", "std/checkedint.d", 1476);
        file_read_detail::allocateBytes([&] { result.resize(used + 16384); });
    }
    if (result.size() - used >= 1024) {
        auto smaller = file_read_detail::allocateBytes([&] {
            return std::vector<std::uint8_t>(result.begin(), result.begin() + used);
        });
        result.swap(smaller);
    } else file_read_detail::allocateBytes([&] { result.resize(used); });
#endif
    return file_read_detail::allocateBytes([&] {
        return common::SharedArray<std::uint8_t>(std::move(result));
    });
}
} // namespace nicxlive::core::d_runtime
