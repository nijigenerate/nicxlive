#pragma once
// Copyright Digital Mars 2000 - 2011. BSL-1.0; see licenses/druntime-throwable.txt.
#include "retained_string.hpp"
#include "exception_message.hpp"
#include <charconv>
#include <array>
#include <type_traits>
#include <utility>
#include <functional>
#include <exception>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>

namespace nicxlive::core::d_runtime {

struct SourceErrorViewTag {};
struct SourceExceptionViewTag {};

// Port of the public Throwable carrier and formatting contract in the installed
// independent LDC druntime object.d. Native throw sites supply source type names;
// implementation-defined C++ RTTI names are never presented as D class names.
class Throwable : public ExceptionMessage {
public:
    using Sink = std::function<void(std::string_view)>;
    class TraceInfo {
    public:
        virtual ~TraceInfo() = default;
        virtual int forEach(const std::function<int(std::string_view)>& visitor) const = 0;
        virtual int forEach(const std::function<int(std::size_t, std::string_view)>& visitor) const = 0;
        virtual RetainedString toString() const = 0;
    };
    using TraceDeallocator = void (*)(std::shared_ptr<TraceInfo>) noexcept;
private:
    // One source class object shared by all native exception transport wrappers.
    struct State {
        std::string_view sourceErrorType;
        std::array<std::size_t, 3> sourceErrorPayload{};
        char* sourceErrorMessage = nullptr;
        const char* legacyErrorMessage = nullptr;
        const char* legacyErrorFile = nullptr;
        unsigned legacyErrorLine = 0;
        RetainedString msg;
        RetainedString file;
        std::size_t line = 0;
        std::shared_ptr<TraceInfo> info;
        TraceDeallocator infoDeallocator = nullptr;
        std::shared_ptr<Throwable> next;
        std::shared_ptr<Throwable> bypassedException;
        State(RetainedString message, RetainedString sourceFile, std::size_t sourceLine,
                std::shared_ptr<Throwable> tail)
            : msg(std::move(message)), file(std::move(sourceFile)), line(sourceLine), next(std::move(tail)) {
            legacyErrorMessage = msg.data(); legacyErrorFile = file.data();
            legacyErrorLine = static_cast<unsigned>(line);
        }
        ~State() {
            next.reset();
            if (infoDeallocator) { infoDeallocator(info); info.reset(); }
        }
    };
    struct StaticErrorStore;
    struct StateSelection { State* state; std::shared_ptr<State> owner; };
    static State*& staticErrorConstructionState() noexcept;
    static StaticErrorStore& staticErrorStore() noexcept;
    static State* staticErrorState() noexcept;
    static StateSelection createState(RetainedString message, RetainedString sourceFile,
        std::size_t sourceLine, std::shared_ptr<Throwable> tail);
    static void appendRenderedText(std::string& output, std::string_view part);
    static RetainedString retainRenderedText(std::string output);
    // Ordinary source class objects own their State; staticError refers directly
    // to the OS thread's TLS object and never creates a shared control block.
    std::shared_ptr<State> stateOwner_;
    State* state_;
    explicit Throwable(StateSelection selected)
        : stateOwner_(std::move(selected.owner)), state_(selected.state),
          msg(state_->msg), file(state_->file), line(state_->line), info(state_->info),
          infoDeallocator(state_->infoDeallocator) {}
public:
    // A construction scope selects the common per-OS-thread source Error object.
    // Ordinary `new Error` equivalents and process-shared ThreadError/SyncError
    // do not enter this scope and retain their existing independent State.
    class StaticErrorConstruction {
        State* previous_;
    public:
        StaticErrorConstruction() noexcept;
        ~StaticErrorConstruction();
        void complete(Throwable& value) noexcept {
            value.state_->sourceErrorType = value.ngThrowableTypeName();
        }
        StaticErrorConstruction(const StaticErrorConstruction&) = delete;
        StaticErrorConstruction& operator=(const StaticErrorConstruction&) = delete;
    };
    // Native owner cleanup at the actual thread-exit boundary, after source
    // users have ended. It never invokes source trace destruction.
    static void ngReleaseStaticErrorThreadStorage() noexcept;
    // Preserve direct-field source access to the actual shared class fields.
    RetainedString& msg;
    RetainedString& file;
    std::size_t& line;
    std::shared_ptr<TraceInfo>& info;
    TraceDeallocator& infoDeallocator;

    explicit Throwable(RetainedString message, std::shared_ptr<Throwable> tail = {})
        : Throwable(std::move(message), RetainedString{}, 0, std::move(tail)) {}
    Throwable(RetainedString message, RetainedString sourceFile, std::size_t sourceLine,
            std::shared_ptr<Throwable> tail = {})
        : Throwable(createState(std::move(message), std::move(sourceFile), sourceLine, std::move(tail))) {}
    Throwable(const Throwable& other) noexcept
        : stateOwner_(other.stateOwner_), state_(other.state_), msg(state_->msg), file(state_->file), line(state_->line), info(state_->info),
          infoDeallocator(state_->infoDeallocator) {}
    Throwable(Throwable&& other) noexcept : Throwable(static_cast<const Throwable&>(other)) {}
    // Source variables assign shared class handles, never native carrier values.
    Throwable& operator=(const Throwable&) = delete;
    Throwable& operator=(Throwable&&) = delete;
    ~Throwable() override = default;
    const void* ngThrowableIdentity() const noexcept { return state_; }
    virtual std::string_view ngThrowableTypeName() const noexcept { return "object.Throwable"; }
    std::string_view ngSourceErrorTypeName(std::string_view fallback) const noexcept {
        return state_->sourceErrorType.empty() ? fallback : state_->sourceErrorType;
    }
    bool ngSourceErrorIsA(std::string_view requested) const noexcept {
        const auto actual = ngThrowableTypeName();
        if (requested == actual) return true;
        if (state_->sourceErrorType.empty()) return false;
        if (requested == "object.Throwable" || requested == "object.Error") return true;
        return requested == "core.exception.RangeError" &&
            (actual == "core.exception.ArrayIndexError" || actual == "core.exception.ArraySliceError");
    }
    virtual std::string_view message() const noexcept { return msg.view(); }
    std::string_view ngExceptionMessage() const noexcept override { return message(); }
    std::shared_ptr<Throwable> next() const noexcept { return state_->next; }
    void next(std::shared_ptr<Throwable> tail) noexcept {
        auto retained = std::move(tail);
        state_->next.reset();
        state_->next = std::move(retained);
    }
    int forEach(const std::function<int(Throwable&)>& visitor) {
        auto* current = this;
        std::shared_ptr<Throwable> retained;
        while (current) {
            [[maybe_unused]] const auto receiverOwner = current->stateOwner_;
            const auto* const state = current->state_;
            if (const auto result = visitor(*current)) return result;
            retained = state->next;
            current = retained.get();
        }
        return 0;
    }
    static std::shared_ptr<Throwable> chainTogether(std::shared_ptr<Throwable> first,
        std::shared_ptr<Throwable> second) {
        if (!first) return second;
        if (!second) return first;
        auto current = first;
        while (current->state_->next) current = current->state_->next;
        current->state_->next = std::move(second);
        return first;
    }
    virtual RetainedString toString() const {
        [[maybe_unused]] const auto receiverOwner = stateOwner_;
        const auto* const state = state_;
        // Source overrides dispatch through the current in-place Error type,
        // including when this native transport predates a later emplacement.
        if (state->msg.empty()) {
            const auto type = ngThrowableTypeName();
            if (type == "core.exception.OutOfMemoryError")
                return RetainedString::fromStaticLiteral("Memory allocation failed");
            if (type == "core.exception.InvalidMemoryOperationError")
                return RetainedString::fromStaticLiteral("Invalid memory operation");
        }
        std::string result;
        auto append = [&](std::string_view part) { appendRenderedText(result, part); };
        // The Source sink is scope: retain the actual local callable by reference.
        toString(Sink(std::ref(append)));
        return retainRenderedText(std::move(result));
    }
    virtual void toString(const Sink& sink) const {
        [[maybe_unused]] const auto receiverOwner = stateOwner_;
        const auto* const state = state_;
        char buffer[20];
        sink(ngThrowableTypeName());
        sink("@"); sink(state->file.view());
        sink("(");
        const auto end = std::to_chars(buffer, buffer + sizeof(buffer), state->line).ptr;
        sink(std::string_view(buffer, static_cast<std::size_t>(end - buffer))); sink(")");
        if (!state->msg.empty()) { sink(": "); sink(state->msg.view()); }
        if (state->info) {
            try {
                sink("\n----------------");
                const auto trace = state->info;
                auto visit = [&](std::string_view frame) {
                    sink("\n"); sink(frame); return 0;
                };
                trace->forEach(std::function<int(std::string_view)>(std::ref(visit)));
            } catch (...) {
                // The source deliberately ignores further Throwable failures.
            }
        }
    }
protected:
    void sourceErrorType(std::string_view type) noexcept { state_->sourceErrorType = type; }
    std::size_t& sourceErrorPayload(std::size_t index) noexcept { return state_->sourceErrorPayload[index]; }
    const char*& sourceErrorMessagePointer() noexcept { return state_->legacyErrorMessage; }
    const char*& sourceErrorFilePointer() noexcept { return state_->legacyErrorFile; }
    unsigned& sourceErrorLineValue() noexcept { return state_->legacyErrorLine; }
    static RetainedString sourceArrayErrorMessage(std::size_t first, std::size_t second,
        std::size_t length, bool slice);
    std::shared_ptr<Throwable>& sourceBypassedException() noexcept { return state_->bypassedException; }
};

// Called inside a native catch: retain the actual thrown object and its dynamic
// type through the runtime exception owner, without slicing a source next handle.
inline std::shared_ptr<Throwable> ngRetainCurrentThrowable(Throwable& value) {
    auto owner = std::make_shared<std::exception_ptr>(std::current_exception());
    if (!*owner) std::terminate();
    return std::shared_ptr<Throwable>(std::move(owner), std::addressof(value));
}

// Separate hierarchies preserve Error versus Exception catch boundaries.
class ThrowableException : public Throwable, public std::exception {
protected:
    // A transport subclass supplies its stable native what() view. The Source
    // Exception object is retained directly; its message cache is not copied.
    ThrowableException(SourceExceptionViewTag, const Throwable& source) noexcept : Throwable(source) {}
    ThrowableException(SourceExceptionViewTag, RetainedString message, RetainedString sourceFile,
        std::size_t sourceLine)
        : Throwable(std::move(message), std::move(sourceFile), sourceLine) {}
public:
    explicit ThrowableException(RetainedString message, std::shared_ptr<Throwable> tail = {},
        std::source_location source = std::source_location::current())
        : Throwable(std::move(message), RetainedString(source.file_name()), source.line(), std::move(tail)),
          nativeMessage_(copyNativeMessage(msg.view())) {}
    ThrowableException(RetainedString message, RetainedString sourceFile, std::size_t sourceLine,
        std::shared_ptr<Throwable> tail = {})
        : Throwable(std::move(message), std::move(sourceFile), sourceLine, std::move(tail)),
          nativeMessage_(copyNativeMessage(msg.view())) {}
    ThrowableException(RetainedString message, std::shared_ptr<Throwable> tail,
        RetainedString sourceFile, std::size_t sourceLine)
        : ThrowableException(std::move(message), std::move(sourceFile), sourceLine, std::move(tail)) {}
    std::string_view ngThrowableTypeName() const noexcept override { return "object.Exception"; }
    const char* what() const noexcept override { return nativeMessage_.c_str(); }
private:
    static std::string copyNativeMessage(std::string_view message);
    std::string nativeMessage_;
};
class ThrowableError : public Throwable {
public:
    using Throwable::Throwable;
    inline static constexpr std::string_view sourceClassName = "object.Error";
    // A source cast/rethrow changes only the native view, never the source State.
    ThrowableError(SourceErrorViewTag, const Throwable& source) noexcept : Throwable(source) {}
    std::shared_ptr<Throwable>& bypassedException = sourceBypassedException();
    ThrowableError(const ThrowableError& other) noexcept
        : Throwable(other), bypassedException(sourceBypassedException()) {}
    ThrowableError(ThrowableError&& other) noexcept
        : Throwable(static_cast<const Throwable&>(other)), bypassedException(sourceBypassedException()) {}
    ThrowableError& operator=(const ThrowableError&) = delete;
    ThrowableError& operator=(ThrowableError&&) = delete;
    std::string_view ngThrowableTypeName() const noexcept override { return ngSourceErrorTypeName("object.Error"); }
};

// Source casts use runtime source class metadata. C++ RTTI describes only the
// transport view and must not be used to cast a retained source Error handle.
template<class Error> std::optional<Error> ngCastSourceError(const Throwable* value) {
    static_assert(std::is_base_of_v<ThrowableError, Error>);
    if (!value || !value->ngSourceErrorIsA(Error::sourceClassName)) return std::nullopt;
    // A nullable stack-held native view, not a newly allocated source object
    // or shared control block. Static State views remain borrowed from TLS.
    return std::optional<Error>(std::in_place, SourceErrorViewTag{}, *value);
}
template<class Error> std::optional<Error> ngCastSourceError(const std::shared_ptr<Throwable>& value) {
    return ngCastSourceError<Error>(value.get());
}

// Byte arguments to staticError are borrowed source descriptors. Their backing
// must survive until the source error stops using it; generated source callers
// pass immutable/static bytes or retain the actual RetainedString owner.
template<class Arg> decltype(auto) ngStaticErrorArgument(Arg&& value) {
    using Value = std::remove_reference_t<Arg>;
    if constexpr (std::is_array_v<Value> &&
        std::is_same_v<std::remove_cv_t<std::remove_extent_t<Value>>, char>) {
        return RetainedString::fromBorrowedView(std::string_view(value, std::extent_v<Value> - 1));
    } else if constexpr (std::is_same_v<std::remove_cvref_t<Arg>, const char*> ||
        std::is_same_v<std::remove_cvref_t<Arg>, char*>) {
        return value ? RetainedString::fromBorrowedView(std::string_view(value)) : RetainedString{};
    } else return std::forward<Arg>(value);
}

// Native transport values share the real source object, not an identity token.
template<class Error, class... Args> Error ngStaticError(Args&&... args) {
    static_assert(std::is_base_of_v<ThrowableError, Error>);
    Throwable::StaticErrorConstruction scope;
    Error result(ngStaticErrorArgument(std::forward<Args>(args))...);
    scope.complete(result);
    return result;
}
}
