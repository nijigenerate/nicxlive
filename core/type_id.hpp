#pragma once

#include "compat/retained_string.hpp"
#include "source_attributes.hpp"

namespace nicxlive::core {

// nijilive.core.TypeId is an attribute value with a D string descriptor.
// Its id is independent of the annotated class's language or RTTI name.
struct TypeId {
    d_runtime::RetainedString id;
};

} // namespace nicxlive::core
