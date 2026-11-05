#pragma once
// Phobos cutoff/power source: Copyright The D Language Foundation 2000-2011, 2019-.
// BSL-1.0; see licenses/druntime-string-hash.txt for the Boost Software License text.

#include <algorithm>
#include <bit>
#include <cfenv>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace nicxlive::core::d_runtime::decimal_format {

// Typed numeric layer, not a parser or a substitute for the full format engine.
// The source contract is LDC 1.41 Phobos std.format.internal.floats/write.
struct DecimalFormatSpec {
    char conversion = 's';
    std::optional<std::size_t> precision;
    std::size_t width = 0;
    bool left = false;
    bool plus = false;
    bool space = false;
    bool zero = false;
    bool alternate = false;
};

namespace detail {
// Selected Phobos std.math.exponential.pow(double, int) uses real intermediates.
inline double formatPowerTen(std::int32_t exponent) {
    // Keep the actual current rounding-mode arithmetic out of constant folding.
    volatile long double product = 1;
    volatile long double factor;
    auto remaining = static_cast<std::uint32_t>(exponent);
    if (exponent < 0) {
        if (exponent == -1) return 1.0 / 10.0;
        remaining = 0u - remaining;
        factor = product / 10.L;
    } else {
        switch (exponent) {
        case 0: return 1;
        case 1: return 10;
        case 2: return 10.0 * 10.0;
        default: break;
        }
        factor = 10;
    }
    for (;;) {
        if (remaining & 1u) product = product * factor;
        remaining >>= 1;
        if (!remaining) break;
        factor = factor * factor;
    }
    return static_cast<double>(product);
}

template<class T>
bool scientificGeneralFormat(T number, std::int32_t precision) {
    const auto magnitude = std::fabs(number);
    const auto lowerExponent = std::bit_cast<std::int32_t>(0u - 4u - static_cast<std::uint32_t>(precision));
    // Preserve source short-circuiting: the lower power is not always evaluated.
    switch (std::fegetround()) {
    case FE_UPWARD:
        return magnitude >= formatPowerTen(precision) - (number > 0 ? 1 : 0)
            || magnitude < 0.0001 - (number > 0 ? formatPowerTen(lowerExponent) : 0);
    case FE_DOWNWARD:
        return magnitude >= formatPowerTen(precision) - (number < 0 ? 1 : 0)
            || magnitude < 0.0001 - (number < 0 ? formatPowerTen(lowerExponent) : 0);
    case FE_TOWARDZERO:
        return magnitude >= formatPowerTen(precision) || magnitude < 0.0001;
    case FE_TONEAREST:
        return magnitude >= formatPowerTen(precision) - 0.5
            || magnitude < 0.0001 - 0.5 * formatPowerTen(lowerExponent);
    default: throw std::runtime_error("Unknown floating point rounding mode");
    }
}

struct ExactFormatDecimal {
    std::string digits;
    std::size_t scale = 0;
    bool negative = false;
    bool infinity = false;
    bool nan = false;
};

inline void multiplyFormatDigits(std::string& digits, unsigned factor) {
    unsigned carry = 0;
    for (auto i = digits.size(); i != 0; --i) {
        const auto value = static_cast<unsigned>(digits[i - 1] - '0') * factor + carry;
        digits[i - 1] = static_cast<char>('0' + value % 10);
        carry = value / 10;
    }
    while (carry) {
        digits.insert(digits.begin(), static_cast<char>('0' + carry % 10));
        carry /= 10;
    }
}

template<class T> requires (std::same_as<T, float> || std::same_as<T, double>)
ExactFormatDecimal exactFormatDecimal(T value) {
    static_assert(std::numeric_limits<T>::is_iec559);
    using Bits = std::conditional_t<std::same_as<T, float>, std::uint32_t, std::uint64_t>;
    constexpr unsigned fractional = std::same_as<T, float> ? 23 : 52;
    constexpr unsigned exponentBits = std::same_as<T, float> ? 8 : 11;
    constexpr int bias = std::same_as<T, float> ? 127 : 1023;
    constexpr Bits fractionMask = (Bits{1} << fractional) - 1;
    constexpr Bits exponentMask = (Bits{1} << exponentBits) - 1;
    const auto bits = std::bit_cast<Bits>(value);
    const auto exponent = static_cast<unsigned>((bits >> fractional) & exponentMask);
    const auto fraction = bits & fractionMask;
    ExactFormatDecimal result;
    result.negative = (bits >> (fractional + exponentBits)) != 0;
    if (exponent == exponentMask) {
        result.infinity = fraction == 0;
        result.nan = fraction != 0;
        return result;
    }
    const auto mantissa = exponent ? (Bits{1} << fractional) | fraction : fraction;
    if (!mantissa) { result.digits = "0"; return result; }
    char bytes[std::numeric_limits<Bits>::digits10 + 2];
    const auto converted = std::to_chars(bytes, bytes + sizeof(bytes), mantissa);
    result.digits.assign(bytes, converted.ptr);
    const auto power = static_cast<int>(exponent ? exponent : 1) - bias - static_cast<int>(fractional);
    if (power >= 0) {
        for (int i = 0; i < power; ++i) multiplyFormatDigits(result.digits, 2);
    } else {
        result.scale = static_cast<std::size_t>(-power);
        for (int i = 0; i < -power; ++i) multiplyFormatDigits(result.digits, 5);
    }
    return result;
}

inline bool incrementRoundedFormat(bool negative, bool remainder, char firstDiscarded,
    bool furtherNonzero, bool oddRetained) {
    if (!remainder) return false;
    switch (std::fegetround()) {
    case FE_UPWARD: return !negative;
    case FE_DOWNWARD: return negative;
    case FE_TOWARDZERO: return false;
    case FE_TONEAREST:
        return firstDiscarded > '5' || (firstDiscarded == '5' && (furtherNonzero || oddRetained));
    default: throw std::runtime_error("Unknown floating point rounding mode");
    }
}

inline void incrementFormatInteger(std::string& digits) {
    for (auto i = digits.size(); i != 0; --i) {
        if (digits[i - 1] != '9') { ++digits[i - 1]; return; }
        digits[i - 1] = '0';
    }
    digits.insert(digits.begin(), '1');
}

// Round the exact decimal integer after retaining keep leading decimal digits.
// Negative keep accounts for implicit zeros before the first nonzero digit.
inline std::string roundFormatDigits(const ExactFormatDecimal& value, std::int64_t keep) {
    const auto& digits = value.digits;
    if (keep >= static_cast<std::int64_t>(digits.size())) {
        auto result = digits;
        result.append(static_cast<std::size_t>(keep) - digits.size(), '0');
        return result;
    }
    const auto retained = keep > 0 ? static_cast<std::size_t>(keep) : 0;
    auto result = retained ? digits.substr(0, retained) : std::string("0");
    const auto first = keep >= 0 ? digits[retained] : '0';
    const auto tailStart = keep >= 0 ? retained + 1 : 0;
    const bool tail = std::any_of(digits.begin() + tailStart, digits.end(), [](char c) { return c != '0'; });
    const bool remainder = first != '0' || tail;
    if (incrementRoundedFormat(value.negative, remainder, first, tail, ((result.back() - '0') & 1) != 0))
        incrementFormatInteger(result);
    return result;
}

inline std::string fixedFormatDigits(std::string digits, std::size_t fractional, bool alternate) {
    if (fractional) {
        if (digits.size() <= fractional) digits.insert(0, fractional - digits.size() + 1, '0');
        digits.insert(digits.size() - fractional, 1, '.');
    } else if (alternate) digits.push_back('.');
    return digits;
}

inline void trimFormatFraction(std::string& text) {
    const auto dot = text.find('.');
    if (dot == std::string::npos) return;
    while (!text.empty() && text.back() == '0') text.pop_back();
    if (!text.empty() && text.back() == '.') text.pop_back();
}

inline std::string formatDecimalExponent(int exponent, bool upper) {
    std::string result(1, upper ? 'E' : 'e');
    result.push_back(exponent < 0 ? '-' : '+');
    const auto magnitude = static_cast<unsigned>(exponent < 0 ? -exponent : exponent);
    char bytes[16];
    const auto converted = std::to_chars(bytes, bytes + sizeof(bytes), magnitude);
    if (converted.ptr - bytes < 2) result.push_back('0');
    result.append(bytes, converted.ptr);
    return result;
}

inline std::string alignFormatDecimal(std::string body, bool negative, bool special, const DecimalFormatSpec& spec) {
    std::string sign = negative ? "-" : spec.plus ? "+" : spec.space ? " " : "";
    const auto length = sign.size() + body.size();
    const auto padding = spec.width > length ? spec.width - length : 0;
    if (spec.left) return sign + body + std::string(padding, ' ');
    if (spec.zero && !special) return sign + std::string(padding, '0') + body;
    return std::string(padding, ' ') + sign + body;
}
} // namespace detail

template<class T> requires (std::same_as<T, float> || std::same_as<T, double>)
std::string ngFormatDecimal(T number, const DecimalFormatSpec& spec = {}) {
    auto value = detail::exactFormatDecimal(number);
    const auto conversion = spec.conversion == 's' ? 'g' : spec.conversion;
    const bool upper = conversion == 'E' || conversion == 'F' || conversion == 'G';
    if (conversion != 'e' && conversion != 'E' && conversion != 'f' && conversion != 'F' &&
        conversion != 'g' && conversion != 'G')
        throw std::invalid_argument("Unsupported decimal floating point conversion");
    if (value.infinity || value.nan) {
        const auto body = value.nan ? (upper ? "NAN" : "nan") : (upper ? "INF" : "inf");
        return detail::alignFormatDecimal(body, value.negative, true, spec);
    }
    const auto requested = spec.precision.value_or(6);
    if (requested > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::length_error("Decimal format precision overflows its source integer domain");
    std::string body;
    if (conversion == 'f' || conversion == 'F') {
        const auto keep = static_cast<std::int64_t>(value.digits.size()) +
            static_cast<std::int64_t>(requested) - static_cast<std::int64_t>(value.scale);
        body = detail::fixedFormatDigits(detail::roundFormatDigits(value, keep), requested, spec.alternate);
    } else {
        const bool general = conversion == 'g' || conversion == 'G';
        const auto significant = general ? std::max<std::size_t>(requested, 1) : requested + 1;
        const bool generalScientific = general &&
            detail::scientificGeneralFormat(number, static_cast<std::int32_t>(significant));
        int exponent = value.digits == "0" ? 0 :
            static_cast<int>(value.digits.size()) - static_cast<int>(value.scale) - 1;
        auto digits = detail::roundFormatDigits(value, static_cast<std::int64_t>(significant));
        if (digits.size() > significant) { digits.pop_back(); ++exponent; }
        // Phobos's general zero path prints no exponent even when it selects E.
        const bool scientific = !general || (generalScientific && number != 0);
        if (scientific) {
            body = digits.substr(0, 1);
            if (digits.size() > 1 || spec.alternate) body.push_back('.');
            body.append(digits.substr(1));
            if (general && !spec.alternate) detail::trimFormatFraction(body);
            body += detail::formatDecimalExponent(exponent, upper);
        } else {
            const int point = exponent + 1;
            if (point <= 0) body = "0." + std::string(static_cast<std::size_t>(-point), '0') + digits;
            else if (static_cast<std::size_t>(point) >= digits.size()) {
                body = digits + std::string(static_cast<std::size_t>(point) - digits.size(), '0');
                if (spec.alternate) body.push_back('.');
            } else {
                body = digits;
                body.insert(static_cast<std::size_t>(point), 1, '.');
            }
            if (!spec.alternate) detail::trimFormatFraction(body);
        }
    }
    return detail::alignFormatDecimal(std::move(body), value.negative, false, spec);
}

} // namespace nicxlive::core::d_runtime::decimal_format
