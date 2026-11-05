#pragma once
// Array allocation overflow: rt/lifetime.d442-506, Digital Mars 2000-2012.
// Boost Software License 1.0; see ../../../licenses/druntime-array-errors.txt.
#include "../../../compat/static_error.hpp"

namespace nicxlive::core::nodes {
[[noreturn]] inline void ngThrowPathDriverAllocationError() {
    throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(
        d_runtime::RetainedString::fromStaticLiteral("rt/lifetime.d"), 479);
}
}
