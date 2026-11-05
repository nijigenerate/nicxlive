#pragma once
// Copyright 2020 Inochi2D Project; 2024 nijigenerate Project. BSD-2-Clause.
// Whole declared nijilive.fmt.io helpers; see ../../LICENSE.
// std.bitmanip: Copyright The D Language Foundation. BSL-1.0;
// see ../licenses/phobos-stdio.txt for the full license.
#include "../common/shared_array.hpp"
#include "../compat/phobos_stdio.hpp"
#include "../compat/static_error.hpp"
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace nicxlive::core::fmt::io {
using File = d_runtime::phobos::stdio::SharedFile;
using ByteArray = common::SharedArray<std::uint8_t>;
using d_runtime::RetainedString;

ByteArray read(File& file, std::size_t length);
ByteArray peek(File& file, std::ptrdiff_t length);
RetainedString readStr(File& file, std::uint32_t length);
RetainedString peekStr(File& file, std::uint32_t length);
void skip(File& file, std::ptrdiff_t length);

// Specialize for a translated Source enum using its actual Source scalar base.
// A C++ enum's native underlying type does not establish its D base type.
template<class T> struct SourceEndianEnum;

namespace detail {
template<class T> inline constexpr bool endianScalar =
    (std::is_integral_v<T> || std::is_same_v<T, float> || std::is_same_v<T, double>) &&
    (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);
template<class T> concept MappedEndianEnum = std::is_enum_v<T> && requires {
    typename SourceEndianEnum<T>::Scalar;
    requires endianScalar<typename SourceEndianEnum<T>::Scalar>;
    { SourceEndianEnum<T>::fromScalar(std::declval<typename SourceEndianEnum<T>::Scalar>()) }
        -> std::same_as<T>;
};
template<class T, bool = MappedEndianEnum<T>> struct EndianStorage { using Type = T; };
template<class T> struct EndianStorage<T, true> { using Type = typename SourceEndianEnum<T>::Scalar; };
template<std::size_t N> using UnsignedOfSize = std::conditional_t<N == 8, std::uint64_t,
    std::conditional_t<N == 4, std::uint32_t, std::conditional_t<N == 2, std::uint16_t, std::uint8_t>>>;
template<class T> T decode(const ByteArray& bytes) {
    using U = UnsignedOfSize<sizeof(T)>;
    U value = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i)
        value = static_cast<U>(value | static_cast<U>(static_cast<U>(bytes[i]) << ((sizeof(T) - i - 1) * 8)));
    if constexpr (std::is_same_v<T, bool>) return value != 0;
    else return std::bit_cast<T>(value);
}
}

template<class T> concept SourceEndianValue = detail::endianScalar<std::remove_cv_t<T>> ||
    detail::MappedEndianEnum<std::remove_cv_t<T>>;

template<SourceEndianValue T> T readValue(File& file) {
    using Value = std::remove_cv_t<T>;
    using Scalar = typename detail::EndianStorage<Value>::Type;
    auto bytes = read(file, sizeof(Scalar));
    if (bytes.size() < sizeof(Scalar)) throw d_runtime::ngStaticError<d_runtime::ArraySliceError>(
        0, sizeof(Scalar), bytes.size(), RetainedString::fromStaticLiteral("nijilive/fmt/io.d"), 20);
    const auto value = detail::decode<Scalar>(bytes);
    if constexpr (detail::MappedEndianEnum<Value>) return SourceEndianEnum<Value>::fromScalar(value);
    else return value;
}

template<SourceEndianValue T> T peekValue(File& file) {
    T value = readValue<T>(file);
    using Scalar = typename detail::EndianStorage<std::remove_cv_t<T>>::Type;
    skip(file, -static_cast<std::ptrdiff_t>(sizeof(Scalar)));
    return value;
}
} // namespace nicxlive::core::fmt::io
