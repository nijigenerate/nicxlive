// Copyright 2020 Inochi2D Project; 2024 nijigenerate Project. BSD-2-Clause.
#include "io.hpp"
#include "../compat/system_error.hpp"
#include <new>
#include <stdexcept>

namespace nicxlive::core::fmt::io {
ByteArray read(File& file, std::size_t length) {
    // Source allocates/zero-initializes before invoking File.rawRead, including closed files.
    ByteArray bytes;
    try { bytes = ByteArray(length, std::uint8_t{0}); }
    catch (const std::bad_alloc&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(
            RetainedString::fromStaticLiteral("rt/lifetime.d"), 479);
    } catch (const std::length_error&) {
        throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(
            RetainedString::fromStaticLiteral("rt/lifetime.d"), 479);
    }
    auto result = file.rawRead(std::span<char>(reinterpret_cast<char*>(bytes.data()), bytes.size()));
    return bytes.slice(0, result.size());
}

RetainedString readStr(File& file, std::uint32_t length) {
    auto bytes = read(file, length);
    if (bytes.isNull()) return {};
    return RetainedString::retainedView(bytes.retentionOwner(),
        reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

RetainedString peekStr(File& file, std::uint32_t length) {
    auto value = readStr(file, length);
    // D first casts uint to int, then negates in the same wrapping 32-bit domain.
    const auto offset = std::bit_cast<std::int32_t>(std::uint32_t{0} - length);
    file.seek(offset, SEEK_CUR);
    return value;
}

ByteArray peek(File& file, std::ptrdiff_t length) {
    auto result = read(file, static_cast<std::size_t>(length));
    using Unsigned = std::make_unsigned_t<std::ptrdiff_t>;
    const auto offset = std::bit_cast<std::ptrdiff_t>(Unsigned{0} - static_cast<Unsigned>(length));
    file.seek(static_cast<std::int64_t>(offset), SEEK_CUR);
    return result;
}

void skip(File& file, std::ptrdiff_t length) { file.seek(static_cast<std::int64_t>(length), SEEK_CUR); }
} // namespace nicxlive::core::fmt::io
