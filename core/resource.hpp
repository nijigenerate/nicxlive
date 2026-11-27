#pragma once
// Object protocol adapted from independent object.d, Digital Mars 2000-2011.
// Boost Software License 1.0; see licenses/druntime-associative-array.txt.

#include "common/nullable_string.hpp"
#include "common/object_key_policy.hpp"
#include "compat/object_metadata.hpp"
#include "compat/throwable.hpp"
#include "compat/native_access_fault.hpp"
#include <cstdint>
#include <cstddef>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <typeinfo>
#include <string>
#include <utility>

namespace nicxlive::core {

void ngRegisterModelResourceClassNames();

class Resource {
public:
    virtual ~Resource() = default;
    virtual d_runtime::RetainedString toString() const {
        ngRegisterModelResourceClassNames();
        return d_runtime::ngObjectClassName(typeid(*this));
    }
    virtual common::NullableString getName() const = 0;
    virtual std::uint32_t getUUID() const = 0;

    // object.d Object protocol: interface handles are adjusted to the actual
    // complete object before identity/hash, never replaced by a UUID or proxy.
    const void* objectIdentity() const noexcept { return dynamic_cast<const void*>(this); }
    virtual std::size_t toHash() {
        const auto address = reinterpret_cast<std::size_t>(objectIdentity());
        return address ^ (address >> 4);
    }
    virtual bool opEquals(std::shared_ptr<Resource> other) {
        return other && objectIdentity() == other->objectIdentity();
    }
    virtual int opCmp(std::shared_ptr<Resource>) {
        ngRegisterModelResourceClassNames();
        const auto className = d_runtime::ngObjectClassName(typeid(*this));
        const auto message = [&] {
            try {
                std::string text = "need opCmp for class ";
                text += className.view();
                return d_runtime::RetainedString(std::move(text));
            } catch (const std::bad_alloc&) {
                throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(std::source_location::current());
            }
        }();
        throw d_runtime::ThrowableException(message,
            d_runtime::RetainedString::fromStaticLiteral("object.d"), 195);
    }
};

inline bool ngResourceObjectEquals(std::shared_ptr<Resource> left, std::shared_ptr<Resource> right) {
    return common::ngClassObjectEquals(std::move(left),std::move(right));
}
inline std::size_t ngResourceInterfaceHash(std::shared_ptr<Resource> value) {
    return value ? value->toHash() : 0;
}
inline bool ngResourceInterfaceKeyEquals(std::shared_ptr<Resource> query, std::shared_ptr<Resource> stored) {
    // Pinned TypeInfo_Interface.equals adjusts both interface vtables before
    // object comparison. A null interface faults here, unlike Object equality.
    if (!query || !stored) d_runtime::ngFailNullAccess(false);
    return ngResourceObjectEquals(query, stored) || query->opCmp(stored) == 0;
}
void ngRegisterModelResourceClassNames();

} // namespace nicxlive::core
