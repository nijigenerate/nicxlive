#pragma once
// core.exception.d 348-456, Sean Kelly 2005-2013; BSL-1.0.
#include "thread_exception.hpp"
namespace nicxlive::core::d_runtime {
class OutOfMemoryError : public ThrowableError {
public:
    inline static constexpr std::string_view sourceClassName = "core.exception.OutOfMemoryError";
    OutOfMemoryError(SourceErrorViewTag tag, const Throwable& source) noexcept : ThrowableError(tag, source) {}
    explicit OutOfMemoryError(std::source_location source = std::source_location::current())
        : OutOfMemoryError(RetainedString::fromBorrowedView(source.file_name()), source.line()) {}
    OutOfMemoryError(RetainedString file, std::size_t line, std::shared_ptr<Throwable> tail = {})
        : OutOfMemoryError(true, std::move(file), line, std::move(tail)) {}
    OutOfMemoryError(bool trace, RetainedString file, std::size_t line, std::shared_ptr<Throwable> tail = {})
        : ThrowableError(RetainedString::fromStaticLiteral("Memory allocation failed"),
            std::move(file), line, std::move(tail)) {
        sourceErrorType(sourceClassName);
        if (!trace) info = SuppressTraceInfo::instance();
    }
    std::string_view ngThrowableTypeName() const noexcept override { return ngSourceErrorTypeName(sourceClassName); }
};
class InvalidMemoryOperationError : public ThrowableError {
public:
    inline static constexpr std::string_view sourceClassName = "core.exception.InvalidMemoryOperationError";
    InvalidMemoryOperationError(SourceErrorViewTag tag, const Throwable& source) noexcept : ThrowableError(tag, source) {}
    InvalidMemoryOperationError(RetainedString file, std::size_t line, std::shared_ptr<Throwable> tail = {})
        : ThrowableError(RetainedString::fromStaticLiteral("Invalid memory operation"),
            std::move(file), line, std::move(tail)) { sourceErrorType(sourceClassName); }
    std::string_view ngThrowableTypeName() const noexcept override { return ngSourceErrorTypeName(sourceClassName); }
};
class ForkError : public ThrowableError {
public:
    inline static constexpr std::string_view sourceClassName = "core.exception.ForkError";
    ForkError(SourceErrorViewTag tag, const Throwable& source) noexcept : ThrowableError(tag, source) {}
    ForkError(RetainedString file, std::size_t line, std::shared_ptr<Throwable> tail = {})
        : ThrowableError(RetainedString::fromStaticLiteral("fork() failed"),
            std::move(file), line, std::move(tail)) { sourceErrorType(sourceClassName); }
    std::string_view ngThrowableTypeName() const noexcept override { return ngSourceErrorTypeName(sourceClassName); }
};
}
