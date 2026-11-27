#include "source_mutex.hpp"
#include <mutex>
#ifndef _WIN32
#include <charconv>
#include <cstdlib>
#include <string_view>
#include <unistd.h>
#endif

namespace nicxlive::core::d_runtime {
namespace {
// Requires the pending shared Throwable public-State correction before source
// reference semantics can be accepted. No source completion is claimed here.
SyncError syncError({}, {}, 0);
std::mutex syncErrorTransport;
#ifndef _WIN32
[[noreturn]] void abortMutex(std::string_view message, std::size_t line) noexcept {
    const auto write = [](std::string_view part) { (void)::write(2, part.data(), part.size()); };
    char digits[3 * sizeof(std::size_t)];
    const auto end = std::to_chars(digits, digits + sizeof(digits), line).ptr;
    write("Aborting from "); write("core/sync/mutex.d"); write("(");
    write({digits, static_cast<std::size_t>(end - digits)}); write(") "); write(message);
    std::abort();
}
#endif
}
[[noreturn]] void ngThrowSourceSyncError(RetainedString message) {
    const std::lock_guard lock(syncErrorTransport);
    syncError.msg = std::move(message);
    throw syncError;
}
SourceMutex::SourceMutex() {
#ifdef _WIN32
    InitializeCriticalSection(&handle_);
#else
    pthread_mutexattr_t attributes;
    if (pthread_mutexattr_init(&attributes) != 0)
        abortMutex("Error: pthread_mutexattr_init failed.", 91);
    if (pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE) != 0)
        abortMutex("Error: pthread_mutexattr_settype failed.", 97);
    if (pthread_mutex_init(&handle_, &attributes) != 0)
        abortMutex("Error: pthread_mutex_init failed.", 100);
    if (pthread_mutexattr_destroy(&attributes) != 0)
        abortMutex("Error: pthread_mutexattr_destroy failed.", 94);
#endif
}
SourceMutex::~SourceMutex() noexcept {
#ifdef _WIN32
    DeleteCriticalSection(&handle_);
#else
    if (pthread_mutex_destroy(&handle_) != 0)
        abortMutex("Error: pthread_mutex_destroy failed.", 152);
#endif
}
void SourceMutex::lock() { lock_nothrow(); }
void SourceMutex::unlock() { unlock_nothrow(); }
bool SourceMutex::tryLock() { return tryLock_nothrow(); }
void SourceMutex::lock_nothrow() {
#ifdef _WIN32
    EnterCriticalSection(&handle_);
#else
    if (pthread_mutex_lock(&handle_) != 0)
        ngThrowSourceSyncError(RetainedString::fromStaticLiteral("Unable to lock mutex."));
#endif
}
void SourceMutex::unlock_nothrow() {
#ifdef _WIN32
    LeaveCriticalSection(&handle_);
#else
    if (pthread_mutex_unlock(&handle_) != 0)
        ngThrowSourceSyncError(RetainedString::fromStaticLiteral("Unable to unlock mutex."));
#endif
}
bool SourceMutex::tryLock_nothrow() {
#ifdef _WIN32
    return TryEnterCriticalSection(&handle_) != 0;
#else
    return pthread_mutex_trylock(&handle_) == 0;
#endif
}
}
