#pragma once
#include "system_error.hpp"
#include <cstddef>
#include <memory>
#include <new>
#include <source_location>
#include <type_traits>
#include <utility>

namespace nicxlive::core::d_runtime {
// A stateless standard allocator: only the memory request translates failures.
// Real object constructors and their callbacks execute outside that boundary.
template<class T> struct SourceAllocator {
    using value_type = T;
    using is_always_equal = std::true_type;
    template<class U> struct rebind { using other = SourceAllocator<U>; };
    SourceAllocator() noexcept = default;
    template<class U> SourceAllocator(const SourceAllocator<U>&) noexcept {}
    T* allocate(std::size_t count) {
        try {
            return std::allocator<T>{}.allocate(count);
        } catch (const std::bad_alloc&) {
            throw ngStaticError<OutOfMemoryError>(std::source_location::current());
        }
    }
    void deallocate(T* pointer, std::size_t count) noexcept {
        std::allocator<T>{}.deallocate(pointer, count);
    }
    template<class U, class... Args> void construct(U* pointer, Args&&... arguments) {
        // A Source class may explicitly friend this allocator for its actual
        // private constructor; allocate_shared still constructs that same class.
        auto* memory = const_cast<void*>(static_cast<const volatile void*>(pointer));
        ::new (memory) U(std::forward<Args>(arguments)...);
    }
    template<class U> bool operator==(const SourceAllocator<U>&) const noexcept { return true; }
};
}
