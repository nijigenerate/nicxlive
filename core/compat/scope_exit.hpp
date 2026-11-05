#pragma once

#include "throwable.hpp"
#include <concepts>
#include <exception>
#include <functional>
#include <type_traits>

namespace nicxlive::core::d_runtime {

// Lower a void operation and its source scope(exit) without executing cleanup twice.
// Throwable priority follows the installed LDC Windows chainExceptions body.
template<class Body, class Cleanup>
    requires (std::invocable<Body&> && std::invocable<Cleanup&> &&
        std::same_as<std::invoke_result_t<Body&>, void> &&
        std::same_as<std::invoke_result_t<Cleanup&>, void>)
void ngRunWithScopeExit(Body&& body, Cleanup&& cleanup) {
    std::exception_ptr pending;
    try { std::invoke(body); }
    catch (...) { pending = std::current_exception(); }

    try { std::invoke(cleanup); }
    catch (Throwable& collateral) {
        if (!pending) throw;
        const auto collateralException = std::current_exception();
        const auto collateralHandle =
            ::nicxlive::core::d_runtime::ngRetainCurrentThrowable(collateral);
        try { std::rethrow_exception(pending); }
        catch (Throwable& initial) {
            const auto initialHandle =
                ::nicxlive::core::d_runtime::ngRetainCurrentThrowable(initial);
            if (!dynamic_cast<ThrowableError*>(&initial)) {
                if (auto error = dynamic_cast<ThrowableError*>(&collateral)) {
                    error->bypassedException = initialHandle;
                    std::rethrow_exception(collateralException);
                }
            }
            Throwable::chainTogether(initialHandle, collateralHandle);
            throw;
        } catch (...) {
            // Foreign native exceptions have no source Throwable chain to append.
            std::rethrow_exception(collateralException);
        }
    }
    if (pending) std::rethrow_exception(pending);
}

} // namespace nicxlive::core::d_runtime
