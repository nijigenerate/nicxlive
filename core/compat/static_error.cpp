// Copyright Sean Kelly 2005-2013. BSL-1.0; see licenses/druntime-array-errors.txt.
#include "throwable.hpp"
#include "source_allocator.hpp"
#include <cstddef>
#include <algorithm>
#include <new>
#include <stdexcept>

namespace nicxlive::core::d_runtime {
// The compiled accessor is unique in nicxlive_common; template consumers do not
// instantiate separate stores in each translation unit or each Error class.
struct Throwable::StaticErrorStore {
    alignas(2 * sizeof(std::size_t)) std::byte bytes[256]{};
    std::array<char, 128> messageBytes{};
    bool initialized = false;
};
Throwable::StaticErrorStore& Throwable::staticErrorStore() noexcept {
    // Trivial direct TLS: no dynamic TLS constructor/destructor registration
    // (whose native runtime bookkeeping can itself allocate on first failure).
    constinit thread_local StaticErrorStore store{};
    static_assert(sizeof(State) <= sizeof(store.bytes));
    static_assert(alignof(State) <= 2 * sizeof(std::size_t));
    return store;
}
Throwable::State*& Throwable::staticErrorConstructionState() noexcept {
    constinit thread_local State* state = nullptr;
    return state;
}
Throwable::State* Throwable::staticErrorState() noexcept {
    auto& store = staticErrorStore();
    if (!store.initialized) {
        auto* state = ::new (static_cast<void*>(store.bytes)) State({}, {}, 0, {});
        state->sourceErrorMessage = store.messageBytes.data();
        store.initialized = true;
        return state;
    }
    return std::launder(reinterpret_cast<State*>(store.bytes));
}
void Throwable::ngReleaseStaticErrorThreadStorage() noexcept {
    auto& store = staticErrorStore();
    if (!store.initialized) return;
    auto* state = std::launder(reinterpret_cast<State*>(store.bytes));
    state->infoDeallocator = nullptr;
    state->~State();
    store.initialized = false;
    staticErrorConstructionState() = nullptr;
}
Throwable::StateSelection Throwable::createState(RetainedString message,
    RetainedString sourceFile, std::size_t sourceLine, std::shared_ptr<Throwable> tail) {
    auto* state = staticErrorConstructionState();
    if (!state) {
        auto owner = std::allocate_shared<State>(SourceAllocator<State>{},
            std::move(message), std::move(sourceFile), sourceLine, std::move(tail));
        return {owner.get(), std::move(owner)};
    }
    // Only the selected Error base uses this State. Any unrelated Exception or
    // Error constructed by the derived constructor keeps ordinary ownership.
    staticErrorConstructionState() = nullptr;
    // Reinitialize the existing object's actual fields. Do not run the old
    // Throwable destructor or its trace deallocator as if this were `delete`.
    state->sourceErrorType = {};
    state->msg = std::move(message);
    state->file = std::move(sourceFile);
    state->line = sourceLine;
    state->legacyErrorMessage = state->msg.data();
    state->legacyErrorFile = state->file.data();
    state->legacyErrorLine = static_cast<unsigned>(sourceLine);
    state->info.reset();
    state->infoDeallocator = nullptr;
    state->next = std::move(tail);
    state->bypassedException.reset();
    return {state, {}};
}
std::string ThrowableException::copyNativeMessage(std::string_view message) {
    // Only passive native what() backing storage is inside this boundary.
    // The actual Throwable constructor and source callbacks execute outside it.
    try {
        return std::string(message);
    } catch (const std::bad_alloc&) {
        throw ngStaticError<OutOfMemoryError>(RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    } catch (const std::length_error&) {
        throw ngStaticError<OutOfMemoryError>(RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    }
}
void Throwable::appendRenderedText(std::string& output, std::string_view part) {
    try {
        output.append(part);
    } catch (const std::bad_alloc&) {
        throw ngStaticError<OutOfMemoryError>(RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    } catch (const std::length_error&) {
        throw ngStaticError<OutOfMemoryError>(RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    }
}
RetainedString Throwable::retainRenderedText(std::string output) {
    try {
        return RetainedString(std::move(output));
    } catch (const std::bad_alloc&) {
        throw ngStaticError<OutOfMemoryError>(RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    } catch (const std::length_error&) {
        throw ngStaticError<OutOfMemoryError>(RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    }
}
RetainedString Throwable::sourceArrayErrorMessage(std::size_t first, std::size_t second,
    std::size_t length, bool slice) {
    auto* state = staticErrorConstructionState();
    std::array<char, 128> local{};
    auto* bytes = state ? state->sourceErrorMessage : local.data();
    // Source index fields occupy two words, slice fields three: their inline
    // message arrays overlap with exactly one word of displacement on reuse.
    auto* begin = bytes + (slice ? sizeof(std::size_t) : 0);
    auto* out = begin;
    const auto put = [&](std::string_view text) {
        for (char value : text) *out++ = value;
    };
    const auto number = [&](std::size_t value) { out = std::to_chars(out, bytes + 128, value).ptr; };
    if (slice) {
        // ArraySliceError.msgBuf is initialized to zero before its constructor.
        std::fill_n(begin, 120, '\0');
        put("slice ["); number(first); put(" .. "); number(second); put("] ");
        if (first > second) put("has a larger lower index than upper index");
        else { put("extends past source array of length "); number(length); }
    } else {
        std::fill_n(begin, 100, '\0');
        put("index ["); number(first); put("] is out of bounds for array of length "); number(second);
    }
    const auto count = static_cast<std::size_t>(out - begin);
    if (!state) {
        try {
            return RetainedString(std::string(begin, count));
        } catch (const std::bad_alloc&) {
            throw ngStaticError<OutOfMemoryError>(RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
        } catch (const std::length_error&) {
            throw ngStaticError<OutOfMemoryError>(RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
        }
    }
    return RetainedString::fromBorrowedView(std::string_view(begin, count));
}
Throwable::StaticErrorConstruction::StaticErrorConstruction() noexcept
    : previous_(staticErrorConstructionState()) {
    staticErrorConstructionState() = staticErrorState();
}
Throwable::StaticErrorConstruction::~StaticErrorConstruction() {
    staticErrorConstructionState() = previous_;
}
}
