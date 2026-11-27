#pragma once
// TypeInfoGeneric!float, druntime rt/util/typeinfo.d213-223/431 (v2.111.0),
// and LDC 1.41 core.internal.hash232-283.
// Copyright Kenji Hara 2014- and Igor Stepanov 2013-2013. BSL-1.0;
// see licenses/druntime-string-hash.txt for the Boost Software License text.
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace nicxlive::core::common {
struct Float32KeyPolicy {
    struct Hash {
        std::size_t operator()(float value) const noexcept {
            if (value == 0.0f) value = 0.0f;
            else if (value != value) value = std::numeric_limits<float>::quiet_NaN();
            return std::bit_cast<std::uint32_t>(value);
        }
    };
    struct Equal {
        bool operator()(float query, float stored) const noexcept {
            // TypeInfo equality is IEEE ==, not cmp3's separate NaN ordering.
            return query == stored;
        }
    };
};
} // namespace nicxlive::core::common
