#pragma once
#include "throwable.hpp"
#include <charconv>
#include <cstddef>
#include <string>

namespace nicxlive::core::d_runtime {
// Preserve the installed druntime RangeError's Error (not Exception) boundary.
class RangeError : public ThrowableError {
public:
    inline static constexpr std::string_view sourceClassName = "core.exception.RangeError";
    RangeError(SourceErrorViewTag tag, const Throwable& source) noexcept : ThrowableError(tag, source) {}
    explicit RangeError(std::source_location source = std::source_location::current())
        : ThrowableError(RetainedString::fromStaticLiteral("Range violation"), RetainedString::fromBorrowedView(source.file_name()), source.line()) { sourceErrorType("core.exception.RangeError"); }
    RangeError(RetainedString sourceFile, std::size_t sourceLine, std::shared_ptr<Throwable> tail = {})
        : ThrowableError(RetainedString::fromStaticLiteral("Range violation"),
            std::move(sourceFile), sourceLine, std::move(tail)) { sourceErrorType("core.exception.RangeError"); }
    std::string_view ngThrowableTypeName() const noexcept override { return ngSourceErrorTypeName("core.exception.RangeError"); }
protected:
    RangeError(RetainedString message, RetainedString sourceFile, std::size_t sourceLine,
        std::shared_ptr<Throwable> tail)
        : ThrowableError(std::move(message), std::move(sourceFile), sourceLine, std::move(tail)) { sourceErrorType("core.exception.RangeError"); }
    static std::string decimal(std::size_t value) {
        char buffer[3 * sizeof(std::size_t)];
        const auto end = std::to_chars(buffer, buffer + sizeof(buffer), value).ptr;
        return std::string(buffer, end);
    }
};
}
