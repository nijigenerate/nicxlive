#pragma once
// Phobos std.string.toStringz body. Copyright The D Language Foundation 2007-.
// BSL-1.0; see ../licenses/phobos-source-c-string.txt.

#include "retained_string.hpp"
#include "static_error.hpp"
#include "../common/shared_array.hpp"
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string_view>

namespace nicxlive::core::d_runtime {
namespace detail {
inline constexpr char sourceCStringEmpty[1] = {'\0'};
}

// The source location is fixed by the caller's independently selected runtime.
// It changes no library out-contract or allocation algorithm at runtime.
template<std::size_t AllocationFailureLine> class SourceCString {
public:
    explicit SourceCString(RetainedString input) : SourceCString(input.view()) {}
    explicit SourceCString(std::string_view input) {
        if (input.empty()) return;
        const auto length = input.size();
        try {
            bytes_ = common::SharedArray<char>(length + 1,
                std::bit_cast<char>(std::uint8_t{0xff}));
        } catch (const std::bad_alloc&) {
            throw ngStaticError<OutOfMemoryError>(
                RetainedString::fromStaticLiteral("rt/lifetime.d"), AllocationFailureLine);
        } catch (const std::length_error&) {
            throw ngStaticError<OutOfMemoryError>(
                RetainedString::fromStaticLiteral("rt/lifetime.d"), AllocationFailureLine);
        }
        std::copy(input.begin(), input.end(), bytes_.data());
        bytes_[length] = '\0';
    }
    const char* data() const noexcept {
        return bytes_.isNull() ? detail::sourceCStringEmpty : bytes_.data();
    }
    RetainedString retainedString() const {
        const auto* bytes = data();
        const auto length = std::strlen(bytes);
        if (bytes_.isNull()) return RetainedString::fromStaticView({bytes, length});
        return RetainedString::retainedView(bytes_.retentionOwner(), bytes, length);
    }
private:
    common::SharedArray<char> bytes_;
};
}
