#pragma once
#include <string_view>

namespace nicxlive::core::d_runtime {
// D Exception.msg is a retained byte string, not a NUL-terminated what() snapshot.
class ExceptionMessage {
public:
    virtual ~ExceptionMessage() = default;
    virtual std::string_view ngExceptionMessage() const noexcept = 0;
};
}
