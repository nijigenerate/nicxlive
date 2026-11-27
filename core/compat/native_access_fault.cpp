#include "native_access_fault.hpp"

#include <cstdlib>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <csignal>
#endif

namespace nicxlive::core::d_runtime {

// Shared production fault boundary for translated null pointer/reference access.
// Native fault context and handler recovery still require platform validation.
[[noreturn]] void ngFailNullAccess(bool writing) {
#if defined(_WIN32)
    const ULONG_PTR information[] = {writing ? ULONG_PTR{1} : ULONG_PTR{0}, 0};
    ::RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 2, information);
#else
    (void)writing;
    std::raise(SIGSEGV);
#endif
    std::abort();
}

}
