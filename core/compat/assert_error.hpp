#pragma once
#include "throwable.hpp"
#include <cstddef>
#include <cstdlib>
#include <utility>

namespace nicxlive::core::d_runtime {

// D AssertError derives from Error, not Exception. Do not make ordinary native
// catch(std::exception&) handlers intercept this source assertion failure.
class AssertError : public ThrowableError {
public:
    inline static constexpr std::string_view sourceClassName = "core.exception.AssertError";
    AssertError(SourceErrorViewTag tag, const Throwable& source) noexcept
        : ThrowableError(tag, source), message(sourceErrorMessagePointer()), sourceFile(sourceErrorFilePointer()),
          sourceLine(sourceErrorLineValue()) {}
    AssertError(const char* text, const char* fileName, unsigned lineNumber)
        : ThrowableError(RetainedString(text), RetainedString(fileName), lineNumber),
          message(sourceErrorMessagePointer()), sourceFile(sourceErrorFilePointer()), sourceLine(sourceErrorLineValue()) { sourceErrorType("core.exception.AssertError"); }
    AssertError(RetainedString text, RetainedString fileName, std::size_t lineNumber,
            std::shared_ptr<Throwable> tail = {})
        : ThrowableError(std::move(text), std::move(fileName), lineNumber, std::move(tail)),
          message(sourceErrorMessagePointer()), sourceFile(sourceErrorFilePointer()), sourceLine(sourceErrorLineValue()) { sourceErrorType("core.exception.AssertError"); }
    std::string_view ngThrowableTypeName() const noexcept override { return ngSourceErrorTypeName("core.exception.AssertError"); }
    // Preserve legacy byte addresses; counted text is available in msg/file.
    // Retained constructor inputs need not end at a NUL byte.
    const char*& message;
    const char*& sourceFile;
    unsigned& sourceLine;
};

// Unlike a conditional assert, a constant-false D assertion is unreachable in
// every configuration. Release must halt rather than fall through the switch.
[[noreturn]] inline void ngAssertUnreachable(const char* message, const char* sourceFile, unsigned sourceLine) {
#ifdef NDEBUG
    (void)message;
    (void)sourceFile;
    (void)sourceLine;
    std::abort();
#else
    throw ngStaticError<AssertError>(message, sourceFile, sourceLine);
#endif
}

// A dynamically built message must retain its allocation after stack unwinding.
[[noreturn]] inline void ngAssertUnreachable(RetainedString message, RetainedString sourceFile,
    std::size_t sourceLine) {
#ifdef NDEBUG
    (void)message;
    (void)sourceFile;
    (void)sourceLine;
    std::abort();
#else
    throw ngStaticError<AssertError>(std::move(message), std::move(sourceFile), sourceLine);
#endif
}

} // namespace nicxlive::core::d_runtime
