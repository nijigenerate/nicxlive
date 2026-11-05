#pragma once
// core.sync.condition, Sean Kelly 2005-2009, BSL-1.0.
// See licenses/druntime-thread-exceptions.txt.
#include "source_mutex.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace nicxlive::core::d_runtime {
using SourceDuration = std::chrono::duration<std::int64_t, std::ratio<1, 10'000'000>>;

class SourceCondition {
public:
    explicit SourceCondition(std::shared_ptr<SourceMutex> mutex);
    virtual ~SourceCondition() noexcept(false);
    SourceCondition(const SourceCondition&) = delete;
    SourceCondition& operator=(const SourceCondition&) = delete;
    virtual std::shared_ptr<SourceMutex> mutex() const;
    std::shared_ptr<SourceMutex> mutex_nothrow() const noexcept;
    virtual void wait();
    virtual bool wait(SourceDuration duration);
    virtual void notify();
    virtual void notifyAll();

private:
    std::shared_ptr<SourceMutex> associatedMutex_;
#ifdef _WIN32
    HANDLE blockLock_ = nullptr;
    HANDLE blockQueue_ = nullptr;
    CRITICAL_SECTION unblockLock_;
    // The source shared-Condition branch uses atomicOp. Native atomics also
    // keep cross-lock counter observations defined for unqualified handles.
    std::atomic<std::int32_t> waitersGone_{0}, waitersBlocked_{0}, waitersToUnblock_{0};
    bool timedWait(DWORD timeout);
    void notify(bool all);
#else
    pthread_cond_t handle_;
#endif
};
}
