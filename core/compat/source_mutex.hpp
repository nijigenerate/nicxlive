#pragma once
// core.sync.mutex / exception, Sean Kelly 2005-2009, BSL-1.0.
#include "throwable.hpp"
#include <memory>
#include <source_location>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace nicxlive::core::d_runtime {
class SyncError : public ThrowableError {
public:
    explicit SyncError(RetainedString message, std::shared_ptr<Throwable> tail = {},
            std::source_location source = std::source_location::current())
        : ThrowableError(std::move(message), RetainedString(source.file_name()), source.line(), std::move(tail)) {}
    SyncError(RetainedString message, RetainedString file, std::size_t line,
            std::shared_ptr<Throwable> tail = {})
        : ThrowableError(std::move(message), std::move(file), line, std::move(tail)) {}
    SyncError(RetainedString message, std::shared_ptr<Throwable> tail,
            RetainedString file, std::size_t line)
        : ThrowableError(std::move(message), std::move(file), line, std::move(tail)) {}
    std::string_view ngThrowableTypeName() const noexcept override { return "core.sync.exception.SyncError"; }
};
[[noreturn]] void ngThrowSourceSyncError(RetainedString message);
class SourceCondition;
class SourceMutex {
    friend class SourceCondition;
#ifdef _WIN32
    CRITICAL_SECTION handle_{};
#else
    pthread_mutex_t handle_{};
#endif
public:
    SourceMutex();
    virtual ~SourceMutex() noexcept;
    SourceMutex(const SourceMutex&) = delete;
    SourceMutex& operator=(const SourceMutex&) = delete;
    virtual void lock();
    virtual void unlock();
    virtual bool tryLock();
    void lock_nothrow();
    void unlock_nothrow();
    bool tryLock_nothrow();
};
}
