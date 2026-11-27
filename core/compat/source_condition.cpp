// Used core.sync.condition/config bodies, Sean Kelly 2005-2009, BSL-1.0.
#include "source_condition.hpp"
#include "assert_error.hpp"
#include "native_access_fault.hpp"
#include "scope_exit.hpp"
#include <bit>
#include <limits>
#include <utility>
#ifndef _WIN32
#include <cerrno>
#include <ctime>
#include <sys/time.h>
#endif

namespace nicxlive::core::d_runtime {
namespace {
[[noreturn]] void failure(const char* message, unsigned line) {
    // The actual source failure uses the same per-OS-thread staticError store.
    throw ngStaticError<AssertError>(message, "core/sync/condition.d", line);
}
void require(bool value, unsigned line, const char* message = "Assertion failure") {
#ifndef NDEBUG
    if (!value) failure(message, line);
#else
    (void)value; (void)line; (void)message;
#endif
}
#ifndef _WIN32
#if !defined(__APPLE__) && !defined(__ANDROID__)
constexpr bool monotonicClock = true;
#else
constexpr bool monotonicClock = false;
#endif
void makeTimespec(timespec& result, SourceDuration delta) {
    if constexpr (monotonicClock) ::clock_gettime(CLOCK_MONOTONIC, &result);
    else {
        timeval now;
        ::gettimeofday(&now, nullptr);
        result = {};
        result.tv_sec = static_cast<decltype(result.tv_sec)>(now.tv_sec);
        result.tv_nsec = static_cast<decltype(result.tv_nsec)>(now.tv_usec) * 1000;
    }
    // core.time.Duration has signed 100-ns ticks. Preserve wrapping integer
    // arithmetic and truncation before the source seconds/nanoseconds split.
    auto ticks = std::bit_cast<std::uint64_t>(delta.count());
    ticks += static_cast<std::uint64_t>(result.tv_sec) * 10'000'000;
    ticks += static_cast<std::uint64_t>(result.tv_nsec / 100);
    const auto total = std::bit_cast<std::int64_t>(ticks);
    const auto seconds = total / 10'000'000;
    const auto nanos = (total % 10'000'000) * 100;
    result.tv_sec = seconds > std::numeric_limits<decltype(result.tv_sec)>::max() ?
        std::numeric_limits<decltype(result.tv_sec)>::max() : static_cast<decltype(result.tv_sec)>(seconds);
    result.tv_nsec = static_cast<decltype(result.tv_nsec)>(nanos);
}
#endif
}

SourceCondition::SourceCondition(std::shared_ptr<SourceMutex> mutex) {
#ifdef _WIN32
    blockLock_ = CreateSemaphoreA(nullptr, 1, 1, nullptr);
    if (!blockLock_) failure("Unable to initialize condition", 109);
    try {
        blockQueue_ = CreateSemaphoreA(nullptr, 0, std::numeric_limits<int>::max(), nullptr);
        if (!blockQueue_) failure("Unable to initialize condition", 114);
        try {
            InitializeCriticalSection(&unblockLock_);
            associatedMutex_ = mutex;
        } catch (...) { CloseHandle(blockQueue_); throw; }
    } catch (...) { CloseHandle(blockLock_); throw; }
#else
    associatedMutex_ = mutex;
#if !defined(__APPLE__) && !defined(__ANDROID__)
    pthread_condattr_t attributes;
    if (::pthread_condattr_init(&attributes)) failure("Unable to initialize condition", 142);
    if (::pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC)) failure("Unable to initialize condition", 145);
    if (::pthread_cond_init(&handle_, &attributes)) failure("Unable to initialize condition", 148);
    if (::pthread_condattr_destroy(&attributes)) failure("Unable to initialize condition", 151);
#else
    if (::pthread_cond_init(&handle_, nullptr)) failure("Unable to initialize condition", 158);
#endif
#endif
}
SourceCondition::~SourceCondition() noexcept(false) {
#ifdef _WIN32
    auto result = CloseHandle(blockLock_);
    require(result != 0, 168, "Unable to destroy condition");
    result = CloseHandle(blockQueue_);
    require(result != 0, 170, "Unable to destroy condition");
    DeleteCriticalSection(&unblockLock_);
#else
    require(::pthread_cond_destroy(&handle_) == 0, 176, "Unable to destroy condition");
#endif
}
std::shared_ptr<SourceMutex> SourceCondition::mutex() const { return associatedMutex_; }
std::shared_ptr<SourceMutex> SourceCondition::mutex_nothrow() const noexcept { return associatedMutex_; }
void SourceCondition::wait() {
#ifdef _WIN32
    timedWait(INFINITE);
#else
    if (::pthread_cond_wait(&handle_, &ngRequireObject(associatedMutex_)->handle_))
        failure("Unable to wait for condition", 251);
#endif
}
bool SourceCondition::wait(SourceDuration duration) {
    require(duration.count() >= 0, 287);
#ifdef _WIN32
    constexpr auto maximumMillis = std::numeric_limits<std::uint32_t>::max() - 1;
    constexpr SourceDuration maximum = std::chrono::milliseconds(maximumMillis);
    while (duration > maximum) {
        if (timedWait(maximumMillis)) return true;
        duration -= maximum;
    }
    return timedWait(static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()));
#else
    timespec deadline;
    makeTimespec(deadline, duration);
    const auto result = ::pthread_cond_timedwait(&handle_, &ngRequireObject(associatedMutex_)->handle_, &deadline);
    if (!result) return true;
    if (result == ETIMEDOUT) return false;
    failure("Unable to wait for condition", 316);
#endif
}
void SourceCondition::notify() {
#ifdef _WIN32
    notify(false);
#else
    int result;
    do { result = ::pthread_cond_signal(&handle_); } while (result == EAGAIN);
    if (result) failure("Unable to notify condition", 364);
#endif
}
void SourceCondition::notifyAll() {
#ifdef _WIN32
    notify(true);
#else
    int result;
    do { result = ::pthread_cond_broadcast(&handle_); } while (result == EAGAIN);
    if (result) failure("Unable to notify condition", 412);
#endif
}

#ifdef _WIN32
bool SourceCondition::timedWait(DWORD timeout) {
    int signalsLeft, waitersGone = 0;
    auto result = WaitForSingleObject(blockLock_, INFINITE);
    require(result == WAIT_OBJECT_0, 443);
    waitersBlocked_.fetch_add(1);
    result = ReleaseSemaphore(blockLock_, 1, nullptr);
    require(result != 0, 448);
    ngRequireObject(associatedMutex_)->unlock();
    bool completed = false, timedOut = false;
    ngRunWithScopeExit([&] {
        result = WaitForSingleObject(blockQueue_, timeout);
        require(result == WAIT_OBJECT_0 || result == WAIT_TIMEOUT, 454);
        timedOut = result == WAIT_TIMEOUT;
        EnterCriticalSection(&unblockLock_);
        try {
            if ((signalsLeft = waitersToUnblock_.load()) != 0) {
                if (timedOut) {
                    if (waitersBlocked_.load() != 0) {
                        waitersBlocked_.fetch_sub(1);
                        signalsLeft = 0;
                    } else waitersGone_ = 1;
                }
                if (waitersToUnblock_.fetch_sub(1) - 1 == 0) {
                    if (waitersBlocked_.load() != 0) {
                        result = ReleaseSemaphore(blockLock_, 1, nullptr);
                        require(result != 0, 483);
                        signalsLeft = 0;
                    } else if ((waitersGone = waitersGone_.load()) != 0) waitersGone_ = 0;
                }
            } else if (waitersGone_.fetch_add(1) + 1 == std::numeric_limits<int>::max() / 2) {
                result = WaitForSingleObject(blockLock_, INFINITE);
                require(result == WAIT_OBJECT_0, 497);
                waitersBlocked_.fetch_sub(waitersGone_.load());
                result = ReleaseSemaphore(blockLock_, 1, nullptr);
                // Preserve the original BOOL-versus-WAIT_OBJECT_0 assertion.
                require(result == WAIT_OBJECT_0, 501);
                waitersGone_ = 0;
            }
            LeaveCriticalSection(&unblockLock_);
            if (signalsLeft == 1) {
                for (; waitersGone > 0; --waitersGone) {
                    result = WaitForSingleObject(blockQueue_, INFINITE);
                    require(result == WAIT_OBJECT_0, 513);
                }
                result = ReleaseSemaphore(blockLock_, 1, nullptr);
                require(result != 0, 517);
            } else if (signalsLeft != 0) {
                result = ReleaseSemaphore(blockQueue_, 1, nullptr);
                require(result != 0, 523);
            }
            ngRequireObject(associatedMutex_)->lock();
            completed = true;
        } catch (...) {
            // The source scope(failure) also covers the explicit unlock and tail.
            LeaveCriticalSection(&unblockLock_);
            throw;
        }
    }, [&] {
        // scope(failure) extends through the final explicit lock as in D.
        if (!completed) ngRequireObject(associatedMutex_)->lock();
    });
    return !timedOut;
}
void SourceCondition::notify(bool all) {
    DWORD result;
    EnterCriticalSection(&unblockLock_);
    try {
        if (waitersToUnblock_.load() != 0) {
            if (waitersBlocked_.load() == 0) {
                LeaveCriticalSection(&unblockLock_);
                return;
            }
            if (all) {
                waitersToUnblock_.fetch_add(waitersBlocked_.load());
                waitersBlocked_ = 0;
            } else {
                waitersToUnblock_.fetch_add(1);
                waitersBlocked_.fetch_sub(1);
            }
            LeaveCriticalSection(&unblockLock_);
        } else if (waitersBlocked_.load() > waitersGone_.load()) {
            result = WaitForSingleObject(blockLock_, INFINITE);
            require(result == WAIT_OBJECT_0, 576);
            if (waitersGone_.load() != 0) {
                waitersBlocked_.fetch_sub(waitersGone_.load());
                waitersGone_ = 0;
            }
            if (all) {
                waitersToUnblock_ = waitersBlocked_.load();
                waitersBlocked_ = 0;
            } else {
                waitersToUnblock_ = 1;
                waitersBlocked_.fetch_sub(1);
            }
            LeaveCriticalSection(&unblockLock_);
            result = ReleaseSemaphore(blockQueue_, 1, nullptr);
            require(result != 0, 594);
        } else LeaveCriticalSection(&unblockLock_);
    } catch (...) {
        // The original scope(failure) persists after its explicit Leave calls.
        LeaveCriticalSection(&unblockLock_);
        throw;
    }
}
#endif
}
