#pragma once
// Copyright Sean Kelly 2005-2013. BSL-1.0; see licenses/druntime-array-errors.txt.
#include "throwable.hpp"
namespace nicxlive::core::d_runtime {
class SwitchError : public ThrowableError {
public:
    inline static constexpr std::string_view sourceClassName = "core.exception.SwitchError";
    SwitchError(SourceErrorViewTag tag, const Throwable& source) noexcept : ThrowableError(tag, source) {}
    SwitchError(RetainedString message, RetainedString sourceFile, std::size_t sourceLine,
        std::shared_ptr<Throwable> tail = {})
        : ThrowableError(std::move(message), std::move(sourceFile), sourceLine, std::move(tail)) {
        sourceErrorType("core.exception.SwitchError");
    }
    explicit SwitchError(RetainedString message, std::source_location source = std::source_location::current(),
        std::shared_ptr<Throwable> tail = {})
        : SwitchError(std::move(message), RetainedString::fromBorrowedView(source.file_name()), source.line(), std::move(tail)) {}
    std::string_view ngThrowableTypeName() const noexcept override {
        return ngSourceErrorTypeName("core.exception.SwitchError");
    }
};
}
