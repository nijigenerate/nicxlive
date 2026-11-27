#pragma once
// Actual mir.serde / fghj.fghj exception hierarchy, not std.conv.ConvException.
// Source: mir-algorithm 3.22.4 and FGHJ 1.0.2 (MIT).
#include "compat/throwable.hpp"
#include <cstddef>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

#ifndef NICXLIVE_MIR_PARSE_SOURCE_FILE
#define NICXLIVE_MIR_PARSE_SOURCE_FILE "mir/parse.d"
#endif
#ifndef NICXLIVE_FGHJ_JSONPARSER_SOURCE_FILE
#define NICXLIVE_FGHJ_JSONPARSER_SOURCE_FILE "fghj/jsonparser.d"
#endif
#ifndef NICXLIVE_FGHJ_SOURCE_FILE
#define NICXLIVE_FGHJ_SOURCE_FILE "fghj/fghj.d"
#endif

namespace nicxlive::core::serde::source_exceptions {
class SerdeException : public d_runtime::ThrowableException {
public:
    using ThrowableException::ThrowableException;
    std::string_view ngThrowableTypeName() const noexcept override { return "mir.serde.SerdeException"; }
};
class FghjSerdeException : public SerdeException {
public:
    std::size_t location = 0;
    explicit FghjSerdeException(std::string_view message, std::size_t offset = 0)
        : SerdeException(d_runtime::RetainedString(message),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_JSONPARSER_SOURCE_FILE), 155),
          location(offset) {}
    FghjSerdeException(d_runtime::RetainedString message, std::size_t offset,
            d_runtime::RetainedString file, std::size_t line,
            std::shared_ptr<d_runtime::Throwable> next = {})
        : SerdeException(std::move(message), std::move(file), line, std::move(next)), location(offset) {}
    std::string_view ngThrowableTypeName() const noexcept override { return "fghj.fghj.FghjSerdeException"; }
};
class EmptyFghjException : public SerdeException {
public:
    EmptyFghjException()
        : SerdeException(d_runtime::RetainedString::fromStaticLiteral("FGHJ value is empty"),
            d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_FGHJ_SOURCE_FILE), 167) {}
    std::string_view ngThrowableTypeName() const noexcept override { return "fghj.fghj.EmptyFghjException"; }
};
} // namespace nicxlive::core::serde::source_exceptions

namespace nicxlive::core::serde::detail {
template<class T> constexpr std::string_view mirNumberTypeName() {
    if constexpr (std::is_same_v<T, float>) return "float";
    else if constexpr (std::is_same_v<T, double>) return "double";
    else if constexpr (std::is_same_v<T, long double>) return "real";
    else if constexpr (sizeof(T) == 1 && std::is_signed_v<T>) return "byte";
    else if constexpr (sizeof(T) == 1) return "ubyte";
    else if constexpr (sizeof(T) == 2 && std::is_signed_v<T>) return "short";
    else if constexpr (sizeof(T) == 2) return "ushort";
    else if constexpr (sizeof(T) == 4 && std::is_signed_v<T>) return "int";
    else if constexpr (sizeof(T) == 4) return "uint";
    else if constexpr (sizeof(T) == 8 && std::is_signed_v<T>) return "long";
    else return "ulong";
}

template<class T, bool remaining> const std::exception_ptr& mirFromStringException() {
    // One real Exception per instantiated source type and failure branch.
    // exception_ptr preserves the runtime owner; Throwable's shared reference
    // identity survives native ABI transport copies of that same D reference.
    static const std::exception_ptr exception = [] {
        try {
            std::string message = remaining
                ? "fromString: remaining input is not empty after parsing "
                : "fromString failed to parse ";
            message += mirNumberTypeName<T>();
            throw d_runtime::ThrowableException(d_runtime::RetainedString(std::move(message)),
                d_runtime::RetainedString::fromStaticLiteral(NICXLIVE_MIR_PARSE_SOURCE_FILE),
                remaining ? 124 : 110);
        } catch (...) { return std::current_exception(); }
    }();
    return exception;
}
template<class T> [[noreturn]] void throwMirFromStringFailure(bool remaining = false) {
    // mir.parse's remaining-input exception exists only in the integral branch.
    if constexpr (!std::is_floating_point_v<T>) {
        if (remaining) std::rethrow_exception(mirFromStringException<T, true>());
    }
    std::rethrow_exception(mirFromStringException<T, false>());
}
[[noreturn]] inline void throwMissingFghjValue() {
    static const auto exception = [] {
        try { throw source_exceptions::EmptyFghjException(); }
        catch (...) { return std::current_exception(); }
    }();
    std::rethrow_exception(exception);
}
} // namespace nicxlive::core::serde::detail
