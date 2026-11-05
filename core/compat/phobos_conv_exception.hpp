#pragma once
// Copyright Digital Mars / The D Language Foundation. BSL-1.0.
// std.conv ConvException / ConvOverflowException; see licenses/phobos-stdio.txt.
#include "throwable.hpp"

namespace nicxlive::core::d_runtime::phobos::conv {
class ConvException : public ThrowableException {
public:
    using ThrowableException::ThrowableException;
    std::string_view ngThrowableTypeName() const noexcept override { return "std.conv.ConvException"; }
};
class ConvOverflowException : public ConvException {
public:
    using ConvException::ConvException;
    std::string_view ngThrowableTypeName() const noexcept override { return "std.conv.ConvOverflowException"; }
};
}
