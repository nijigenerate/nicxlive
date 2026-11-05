#pragma once
namespace nicxlive::core::d_runtime {
// The one compiled fault provider owns platform headers and native dispatch.
[[noreturn]] void ngFailNullAccess(bool writing);
template<class Handle> Handle ngRequireObject(Handle value) {
    if (!value) ngFailNullAccess(false);
    return value;
}
}
