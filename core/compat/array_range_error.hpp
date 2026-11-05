#pragma once
// Copyright Sean Kelly 2005-2013. BSL-1.0; see licenses/druntime-array-errors.txt.
// Observable core.exception contracts use the existing actual Throwable carrier.
#include "range_error.hpp"

namespace nicxlive::core::d_runtime {
class ArrayIndexError : public RangeError {
public:
    inline static constexpr std::string_view sourceClassName = "core.exception.ArrayIndexError";
    ArrayIndexError(SourceErrorViewTag tag, const Throwable& source) noexcept
        : RangeError(tag, source), index(sourceErrorPayload(0)), length(sourceErrorPayload(1)) {}
    const std::size_t& index;
    const std::size_t& length;
    ArrayIndexError(std::size_t index, std::size_t length, RetainedString sourceFile,
        std::size_t sourceLine, std::shared_ptr<Throwable> tail = {})
        : RangeError(sourceArrayErrorMessage(index, length, 0, false),
            std::move(sourceFile), sourceLine, std::move(tail)), index(sourceErrorPayload(0)), length(sourceErrorPayload(1)) {
        sourceErrorPayload(0) = index; sourceErrorPayload(1) = length;
        sourceErrorType("core.exception.ArrayIndexError");
    }
    ArrayIndexError(std::size_t index, std::size_t length,
        std::source_location source = std::source_location::current(), std::shared_ptr<Throwable> tail = {})
        : ArrayIndexError(index, length, RetainedString::fromBorrowedView(source.file_name()), source.line(), std::move(tail)) {}
    std::string_view ngThrowableTypeName() const noexcept override { return ngSourceErrorTypeName("core.exception.ArrayIndexError"); }
};

class ArraySliceError : public RangeError {
public:
    inline static constexpr std::string_view sourceClassName = "core.exception.ArraySliceError";
    ArraySliceError(SourceErrorViewTag tag, const Throwable& source) noexcept
        : RangeError(tag, source), lower(sourceErrorPayload(0)), upper(sourceErrorPayload(1)),
          length(sourceErrorPayload(2)) {}
    const std::size_t& lower;
    const std::size_t& upper;
    const std::size_t& length;
    ArraySliceError(std::size_t lower, std::size_t upper, std::size_t length, RetainedString sourceFile,
        std::size_t sourceLine, std::shared_ptr<Throwable> tail = {})
        : RangeError(sourceArrayErrorMessage(lower, upper, length, true),
            std::move(sourceFile), sourceLine, std::move(tail)), lower(sourceErrorPayload(0)), upper(sourceErrorPayload(1)), length(sourceErrorPayload(2)) {
        sourceErrorPayload(0) = lower; sourceErrorPayload(1) = upper; sourceErrorPayload(2) = length;
        sourceErrorType("core.exception.ArraySliceError");
    }
    ArraySliceError(std::size_t lower, std::size_t upper, std::size_t length,
        std::source_location source = std::source_location::current(), std::shared_ptr<Throwable> tail = {})
        : ArraySliceError(lower, upper, length, RetainedString::fromBorrowedView(source.file_name()), source.line(), std::move(tail)) {}
    std::string_view ngThrowableTypeName() const noexcept override { return ngSourceErrorTypeName("core.exception.ArraySliceError"); }
private:
    static std::string makeMessage(std::size_t lower, std::size_t upper, std::size_t length) {
        return "slice [" + decimal(lower) + " .. " + decimal(upper) + "] " +
            (lower > upper ? "has a larger lower index than upper index" :
                "extends past source array of length " + decimal(length));
    }
};
}
