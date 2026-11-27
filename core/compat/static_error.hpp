#pragma once
#include "assert_error.hpp"
#include "array_range_error.hpp"
#include "switch_error.hpp"
#include "system_error.hpp"

namespace nicxlive::core::d_runtime {
// Rethrow a retained canonical source Error after another staticError has reused
// its storage. The current State determines the catch-visible native view.
[[noreturn]] inline void ngRethrowSourceError(const ThrowableError& source) {
    const auto type = source.ngThrowableTypeName();
    if (type == ArrayIndexError::sourceClassName) throw ArrayIndexError(SourceErrorViewTag{}, source);
    if (type == ArraySliceError::sourceClassName) throw ArraySliceError(SourceErrorViewTag{}, source);
    if (type == AssertError::sourceClassName) throw AssertError(SourceErrorViewTag{}, source);
    if (type == RangeError::sourceClassName) throw RangeError(SourceErrorViewTag{}, source);
    if (type == SwitchError::sourceClassName) throw SwitchError(SourceErrorViewTag{}, source);
    if (type == OutOfMemoryError::sourceClassName) throw OutOfMemoryError(SourceErrorViewTag{}, source);
    if (type == InvalidMemoryOperationError::sourceClassName) throw InvalidMemoryOperationError(SourceErrorViewTag{}, source);
    if (type == ForkError::sourceClassName) throw ForkError(SourceErrorViewTag{}, source);
    // The common object.Error base retains source metadata for other Error
    // subclasses; source-specific catch views must be supplied by their owner.
    throw ThrowableError(SourceErrorViewTag{}, source);
}
}
