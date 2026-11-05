// core/thread/threadbase.d 1246-1253 and core/exception.d 907-916.
#include "thread_exception.hpp"
#include <mutex>

namespace nicxlive::core::d_runtime {
namespace {
// Process shared, not TLS and not lazily allocated on the failure path.
const auto suppressedTrace = std::make_shared<SuppressTraceInfo>();
ThreadError threadError({}, RetainedString::fromStaticLiteral("core/thread/threadbase.d"), 1248);
std::mutex threadErrorMutex;
}
std::shared_ptr<SuppressTraceInfo> SuppressTraceInfo::instance() noexcept {
    return suppressedTrace;
}
[[noreturn]] void ngOnThreadError(RetainedString message) {
    // Synchronize native transport copying of the source process-shared object.
    // The inherited source identity token is retained by that transport copy.
    std::lock_guard lock(threadErrorMutex);
    threadError.msg = std::move(message);
    threadError.next({});
    threadError.info = SuppressTraceInfo::instance();
    throw threadError;
}
[[noreturn]] void ngThrowThreadException(RetainedString message,
        RetainedString sourceFile, std::size_t sourceLine) {
    throw ThreadException(std::move(message), std::move(sourceFile), sourceLine);
}
} // namespace nicxlive::core::d_runtime
