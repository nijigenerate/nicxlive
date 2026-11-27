#pragma once

#include "../common/shared_array.hpp"
#include "../compat/static_error.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

namespace nicxlive::core::fmt {

inline const std::array<std::uint8_t, 8> MAGIC_BYTES{0x54, 0x52, 0x4E, 0x53, 0x52, 0x54, 0x53, 0x00};
inline const std::array<std::uint8_t, 8> TEX_SECTION{0x54, 0x45, 0x58, 0x5F, 0x53, 0x45, 0x43, 0x54};
inline const std::array<std::uint8_t, 8> EXT_SECTION{0x45, 0x58, 0x54, 0x5F, 0x53, 0x45, 0x43, 0x54};

inline bool inVerifySection(std::span<const std::uint8_t> buffer, std::span<const std::uint8_t> section) {
    return buffer.size() >= section.size() && std::equal(section.begin(), section.end(), buffer.begin());
}

inline bool inVerifySection(common::SharedArray<std::uint8_t> buffer,
    common::SharedArray<std::uint8_t> section) {
    return inVerifySection(std::span<const std::uint8_t>(buffer.data(), buffer.size()),
        std::span<const std::uint8_t>(section.data(), section.size()));
}

inline bool inVerifySection(const std::vector<std::uint8_t>& buffer,
    const std::array<std::uint8_t, 8>& section) {
    return inVerifySection(std::span<const std::uint8_t>(buffer), std::span<const std::uint8_t>(section));
}

inline bool inVerifyMagicBytes(std::span<const std::uint8_t> buffer) {
    return inVerifySection(buffer, MAGIC_BYTES);
}

inline bool inVerifyMagicBytes(common::SharedArray<std::uint8_t> buffer) {
    return inVerifyMagicBytes(std::span<const std::uint8_t>(buffer.data(), buffer.size()));
}

inline bool inVerifyMagicBytes(const std::vector<std::uint8_t>& buffer) {
    return inVerifyMagicBytes(std::span<const std::uint8_t>(buffer));
}

namespace detail {
template<class T, bool = std::is_enum_v<T>> struct BinaryScalarBase { using Type = T; };
template<class T> struct BinaryScalarBase<T, true> { using Type = std::underlying_type_t<T>; };
template<class T> using BinaryScalar = typename BinaryScalarBase<T>::Type;

template<class T> concept SourceBinaryScalar = !std::is_const_v<T> && !std::is_volatile_v<T> &&
    (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8) &&
    (std::is_integral_v<BinaryScalar<T>> || std::is_same_v<T, float> || std::is_same_v<T, double>);
}

// The unsigned assembly and floating-point reinterpretation adapt Phobos
// std.bitmanip.bigEndianToNative. See ../licenses/phobos-binfmt-source.txt.
template<detail::SourceBinaryScalar T>
inline std::size_t inInterpretDataFromBuffer(std::span<const std::uint8_t> buffer, T& data) {
    if (buffer.size() < sizeof(T)) {
        throw d_runtime::ngStaticError<d_runtime::ArraySliceError>(0, sizeof(T), buffer.size(),
            d_runtime::RetainedString::fromStaticLiteral("source/nijilive/fmt/binfmt.d"), 30);
    }
    std::array<std::uint8_t, sizeof(T)> toInterp;
    std::copy_n(buffer.begin(), sizeof(T), toInterp.begin());
    // Snapshot the complete source slice before committing the output, including
    // when the caller's output object overlaps the original byte buffer.
    std::uint64_t bits = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        bits |= static_cast<std::uint64_t>(toInterp[i]) << ((sizeof(T) - i - 1) * 8);
    }
    if constexpr (std::is_same_v<T, float>) {
        data = std::bit_cast<float>(static_cast<std::uint32_t>(bits));
    } else if constexpr (std::is_same_v<T, double>) {
        data = std::bit_cast<double>(bits);
    } else {
        data = static_cast<T>(static_cast<detail::BinaryScalar<T>>(bits));
    }
    return sizeof(T);
}

template<detail::SourceBinaryScalar T>
inline std::size_t inInterpretDataFromBuffer(common::SharedArray<std::uint8_t> buffer, T& data) {
    return inInterpretDataFromBuffer(std::span<const std::uint8_t>(buffer.data(), buffer.size()), data);
}

template<detail::SourceBinaryScalar T>
inline std::size_t inInterpretDataFromBuffer(const std::vector<std::uint8_t>& buffer, T& data) {
    return inInterpretDataFromBuffer(std::span<const std::uint8_t>(buffer), data);
}

} // namespace nicxlive::core::fmt
