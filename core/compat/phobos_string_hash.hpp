#pragma once
// Adapted from the permitted LDC 1.41 core.internal.hash string byte-hash path.
// Copyright Igor Stepanov 2013-2013; Boost Software License 1.0.
// MurmurHash3 by Austin Appleby is public domain. See druntime-string-hash.txt.
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace nicxlive::core::d_runtime {
inline std::size_t ngPhobosStringHash(std::string_view value) noexcept {
    static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big);
    constexpr std::uint32_t c1 = 0xcc9e2d51u;
    constexpr std::uint32_t c2 = 0x1b873593u;
    std::uint32_t result = 0;
    std::size_t offset = 0;
    const auto byte = [&](std::size_t index) { return static_cast<std::uint32_t>(
        static_cast<unsigned char>(value[index])); };
    while (value.size() - offset >= 4) {
        std::uint32_t block;
        if constexpr (std::endian::native == std::endian::big)
            block = (byte(offset) << 24) | (byte(offset + 1) << 16) | (byte(offset + 2) << 8) | byte(offset + 3);
        else block = byte(offset) | (byte(offset + 1) << 8) | (byte(offset + 2) << 16) | (byte(offset + 3) << 24);
        block *= c1;
        block = std::rotl(block, 15);
        block *= c2;
        result ^= block;
        result = std::rotl(result, 13);
        result = result * 5u + 0xe6546b64u;
        offset += 4;
    }
    std::uint32_t tail = 0;
    switch (value.size() - offset) {
    case 3: tail ^= byte(offset + 2) << 16; [[fallthrough]];
    case 2: tail ^= byte(offset + 1) << 8; [[fallthrough]];
    case 1:
        tail ^= byte(offset);
        tail *= c1;
        tail = std::rotl(tail, 15);
        tail *= c2;
        result ^= tail;
        break;
    default: break;
    }
    result ^= static_cast<std::uint32_t>(value.size());
    result = (result ^ (result >> 16)) * 0x85ebca6bu;
    result = (result ^ (result >> 13)) * 0xc2b2ae35u;
    result ^= result >> 16;
    return result;
}
}
