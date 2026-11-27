#pragma once
// Used const(char)[] std.file.write body, D Language Foundation 2007-2011.
// BSL-1.0; see licenses/phobos-file-write-source.txt.
#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif
#include "retained_string.hpp"
#include "phobos_file_exception.hpp"
#include "phobos_temp_cstring.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <string_view>
#if defined(_WIN32)
#include <Windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace nicxlive::core::d_runtime::phobos {
inline void ngFileWrite(RetainedString name, std::string_view buffer) {
    // Keep the original counted name alive through conversion, I/O and failure.
    const auto fail = [&](std::size_t line) {
#if defined(_WIN32)
        const auto code = static_cast<std::uint32_t>(GetLastError());
#else
        const auto code = static_cast<std::uint32_t>(errno);
#endif
        FileException error(name, code);
        error.file = RetainedString::fromStaticLiteral("std/file.d");
        error.line = line;
        throw error;
    };
#if defined(_WIN32)
    const TempCStringBuffer<wchar_t> path(name.view());
    const auto handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (handle == INVALID_HANDLE_VALUE) fail(885);
    std::size_t sum = 0;
    DWORD written;
    while (sum != buffer.size()) {
        const auto count = (std::min<std::size_t>)(buffer.size() - sum, std::size_t{1} << 30);
        WriteFile(handle, buffer.data() + sum, static_cast<DWORD>(count), &written, nullptr);
        if (written != count) break;
        sum += written;
    }
    // Source short-circuits CloseHandle on a short write; do not add cleanup.
    if (!(sum == buffer.size() && CloseHandle(handle))) fail(898);
#else
    const TempCStringBuffer<char> path(name.view());
    const auto descriptor = ::open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (descriptor == -1) fail(840);
    try {
        std::size_t sum = 0;
        while (sum != buffer.size()) {
            const auto count = (std::min<std::size_t>)(buffer.size() - sum, std::size_t{1} << 30);
            const auto written = ::write(descriptor, buffer.data() + sum, count);
            if (written != static_cast<decltype(written)>(count)) break;
            sum += static_cast<std::size_t>(written);
        }
        if (sum != buffer.size()) fail(854);
    } catch (...) {
        (void)::close(descriptor);
        throw;
    }
    if (::close(descriptor) != 0) fail(856);
#endif
}
} // namespace nicxlive::core::d_runtime::phobos
