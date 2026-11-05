#pragma once
// core.thread.threadbase / core.exception, Sean Kelly 2005-2013, BSL-1.0.
// See licenses/druntime-thread-exceptions.txt.
#include "throwable.hpp"
#include <source_location>
#include <utility>

namespace nicxlive::core::d_runtime {
class ThreadException : public ThrowableException {
public:
    explicit ThreadException(RetainedString message, std::shared_ptr<Throwable> tail = {},
            std::source_location source = std::source_location::current())
        : ThrowableException(std::move(message), std::move(tail), source) {}
    ThreadException(RetainedString message, RetainedString sourceFile, std::size_t sourceLine,
            std::shared_ptr<Throwable> tail = {})
        : ThrowableException(std::move(message), std::move(sourceFile), sourceLine, std::move(tail)) {}
    ThreadException(RetainedString message, std::shared_ptr<Throwable> tail,
            RetainedString sourceFile, std::size_t sourceLine)
        : ThrowableException(std::move(message), std::move(sourceFile), sourceLine, std::move(tail)) {}
    std::string_view ngThrowableTypeName() const noexcept override {
        return "core.thread.threadbase.ThreadException";
    }
};
class ThreadError : public ThrowableError {
public:
    explicit ThreadError(RetainedString message, std::shared_ptr<Throwable> tail = {},
            std::source_location source = std::source_location::current())
        : ThrowableError(std::move(message), RetainedString(source.file_name()), source.line(), std::move(tail)) {}
    ThreadError(RetainedString message, RetainedString sourceFile, std::size_t sourceLine,
            std::shared_ptr<Throwable> tail = {})
        : ThrowableError(std::move(message), std::move(sourceFile), sourceLine, std::move(tail)) {}
    ThreadError(RetainedString message, std::shared_ptr<Throwable> tail,
            RetainedString sourceFile, std::size_t sourceLine)
        : ThrowableError(std::move(message), std::move(sourceFile), sourceLine, std::move(tail)) {}
    std::string_view ngThrowableTypeName() const noexcept override {
        return "core.thread.threadbase.ThreadError";
    }
};
class SuppressTraceInfo final : public Throwable::TraceInfo {
public:
    int forEach(const std::function<int(std::string_view)>&) const override { return 0; }
    int forEach(const std::function<int(std::size_t, std::string_view)>&) const override { return 0; }
    RetainedString toString() const override { return {}; }
    static std::shared_ptr<SuppressTraceInfo> instance() noexcept;
};

// The D helper is nothrow because it throws Error. Native noexcept would
// terminate instead of preserving that source Error catch boundary.
[[noreturn]] void ngOnThreadError(RetainedString message);
[[noreturn]] void ngThrowThreadException(RetainedString message,
    RetainedString sourceFile, std::size_t sourceLine);
} // namespace nicxlive::core::d_runtime
