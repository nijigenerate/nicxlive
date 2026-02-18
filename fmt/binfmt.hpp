#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace nicxlive::core::fmt {

// Magic bytes/section markers matching nijilive fmt/binfmt.d
inline const std::array<uint8_t, 8> MAGIC_BYTES{0x54, 0x52, 0x4E, 0x53, 0x52, 0x54, 0x53, 0x00}; // "TRNSRTS\0"
inline const std::array<uint8_t, 8> TEX_SECTION{0x54, 0x45, 0x58, 0x5F, 0x53, 0x45, 0x43, 0x54}; // "TEX_SECT"
inline const std::array<uint8_t, 8> EXT_SECTION{0x45, 0x58, 0x54, 0x5F, 0x53, 0x45, 0x43, 0x54}; // "EXT_SECT"

inline bool inVerifyMagicBytes(const std::vector<uint8_t>& data) {
    return data.size() >= MAGIC_BYTES.size() &&
           std::equal(MAGIC_BYTES.begin(), MAGIC_BYTES.end(), data.begin());
}

inline bool inVerifySection(const std::vector<uint8_t>& data, const std::array<uint8_t, 8>& section) {
    return data.size() >= section.size() &&
           std::equal(section.begin(), section.end(), data.begin());
}

// Interpret big-endian bytes into native type.
template <typename T>
inline void inInterpretDataFromBuffer(const std::vector<uint8_t>& buffer, T& out) {
    out = 0;
    for (size_t i = 0; i < sizeof(T) && i < buffer.size(); ++i) {
        out = static_cast<T>((out << 8) | buffer[i]);
    }
}

} // namespace nicxlive::core::fmt
