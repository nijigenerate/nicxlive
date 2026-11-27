#pragma once
#include "../common/resource_key_map.hpp"
#include "../common/object_key_policy.hpp"
#include "../compat/source_allocator.hpp"
#include "../compat/object_metadata.hpp"
// Independent nijilive.utils.snapshot source implementation.
#include <puppet.hpp>
#include <nodes/composite.hpp>
#include <common/shared_array.hpp>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace nicxlive::utils {
namespace detail {
template<class T> inline constexpr bool isSnapshotStdFunction = false;
template<class R, class... Args>
inline constexpr bool isSnapshotStdFunction<std::function<R(Args...)>> = true;
} // namespace detail

class Snapshot;
struct SourceSnapshotConstructionTag {};
template<class Complete = Snapshot>
std::shared_ptr<Complete> ngCreateSnapshotSource(std::shared_ptr<core::Puppet> puppet);

class Snapshot : public std::enable_shared_from_this<Snapshot> {
public:
    using Texture = core::Texture;
    using Puppet = core::Puppet;
    using Composite = core::nodes::Composite;

    // Copies retain one mutable delegate context, rather than cloning it.
    class CaptureCallback {
        using Function = std::function<void(std::shared_ptr<Texture>)>;
    public:
        using Invoker = void (*)(void*, std::shared_ptr<Texture>);
        using FunctionPointer = void (*)(std::shared_ptr<Texture>);
        CaptureCallback() noexcept = default;
        CaptureCallback(std::nullptr_t) noexcept {}
        CaptureCallback(FunctionPointer function) noexcept : direct_(function) {}
        template<class Callable> requires (
            !std::same_as<std::remove_cvref_t<Callable>, CaptureCallback> &&
            std::constructible_from<Function, Callable>)
        CaptureCallback(Callable&& callable) {
            using Target = std::decay_t<Callable>;
            if constexpr (std::same_as<Target, FunctionPointer>) {
                direct_ = callable;
            } else if constexpr (detail::isSnapshotStdFunction<Target>) {
                // Native std::function conversion/copy can invoke a target's
                // copy constructor. Keep it outside memory-only translation.
                Function function(std::forward<Callable>(callable));
                if (!function) return;
                context_ = std::allocate_shared<Function>(
                    core::d_runtime::SourceAllocator<Function>{}, std::move(function));
                invoke_ = [](void* context, std::shared_ptr<Texture> texture) {
                    (*static_cast<Function*>(context))(std::move(texture));
                };
            } else {
                if constexpr (std::is_pointer_v<Target> || std::is_member_pointer_v<Target>)
                    if (!callable) return;
                // One real callable context. Descriptor copies retain this
                // same target; its constructor runs outside allocator catches.
                context_ = std::allocate_shared<Target>(
                    core::d_runtime::SourceAllocator<Target>{}, std::forward<Callable>(callable));
                invoke_ = [](void* context, std::shared_ptr<Texture> texture) {
                    std::invoke(*static_cast<Target*>(context), std::move(texture));
                };
            }
        }
        CaptureCallback(const CaptureCallback&) noexcept = default;
        CaptureCallback& operator=(const CaptureCallback&) noexcept = default;
        CaptureCallback(CaptureCallback&& other) noexcept
            : CaptureCallback(static_cast<const CaptureCallback&>(other)) {}
        CaptureCallback& operator=(CaptureCallback&& other) noexcept {
            return *this = static_cast<const CaptureCallback&>(other);
        }
        static CaptureCallback fromContext(std::shared_ptr<void> context, Invoker invoke) noexcept {
            CaptureCallback result;
            result.context_ = std::move(context);
            result.invoke_ = invoke;
            return result;
        }
        template<auto Method, class Context>
        static CaptureCallback fromMethod(const std::shared_ptr<Context>& context) noexcept {
            if (!context) return {};
            return fromContext(context, [](void* receiver, std::shared_ptr<Texture> texture) {
                std::invoke(Method, static_cast<Context*>(receiver), std::move(texture));
            });
        }
        explicit operator bool() const noexcept { return invoke_ || direct_; }
        bool operator==(const CaptureCallback& other) const noexcept {
            return context_.get() == other.context_.get() && invoke_ == other.invoke_ && direct_ == other.direct_;
        }
        void operator()(std::shared_ptr<Texture> texture) const {
            const auto context = context_;
            const auto invoke = invoke_;
            const auto direct = direct_;
            if (direct) { direct(std::move(texture)); return; }
            if (!invoke) throw std::bad_function_call();
            invoke(context.get(), std::move(texture));
        }
    private:
        std::shared_ptr<void> context_;
        Invoker invoke_ = nullptr;
        FunctionPointer direct_ = nullptr;
    };

    Snapshot(const Snapshot&) = delete;
    Snapshot& operator=(const Snapshot&) = delete;
    Snapshot(Snapshot&&) = delete;
    Snapshot& operator=(Snapshot&&) = delete;
    virtual ~Snapshot() = default;
    // Storage-only entry; Source constructor bodies run after complete ownership.
    explicit Snapshot(SourceSnapshotConstructionTag);
    static std::shared_ptr<Snapshot> get(std::shared_ptr<Puppet> puppet);
    virtual void release();
    virtual void onCaptured(CaptureCallback callback);
    virtual void capture();
    static void requestCapture(std::shared_ptr<Snapshot> snapshot, CaptureCallback callback);
    static void processPending();
    virtual core::math::Vec2 position();
protected:
    template<class Complete>
    friend std::shared_ptr<Complete> ngCreateSnapshotSource(std::shared_ptr<core::Puppet>);
    void initializeSnapshotOwned(std::shared_ptr<Puppet> puppet) {
        sharedCount = 1;
        setup(puppet);
    }
    Snapshot();
    // Legacy native constructor; the actual Source factory does not replay it.
    explicit Snapshot(std::shared_ptr<Puppet> puppet);
    virtual void setup(std::shared_ptr<Puppet> puppet);
    std::shared_ptr<Puppet> snapshotPuppet;
    std::shared_ptr<Composite> dcomposite;
    std::int32_t sharedCount = 0;
    core::common::SharedArray<CaptureCallback> pendingCallbacks;
    struct PendingRequest { std::shared_ptr<Snapshot> snap; CaptureCallback cb; };
    using HandleMap = core::common::PolicyKeyMap<std::shared_ptr<Puppet>, std::shared_ptr<Snapshot>,
        core::common::ObjectClassKeyPolicy<std::shared_ptr<Puppet>>>;
    static thread_local HandleMap handles;
    static thread_local core::common::SharedArray<PendingRequest> pendingQueue;

public:
    // Inherited Source Object protocol; append after existing virtual slots.
    virtual core::d_runtime::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<Snapshot> other);
    virtual int opCmp(std::shared_ptr<Snapshot> other);
};

bool ngSnapshotObjectEquals(std::shared_ptr<Snapshot> left, std::shared_ptr<Snapshot> right);

// Derived translations supply a storage-only tag constructor and perform their
// own Source body after this original base body returns, on the same object.
template<class Complete>
std::shared_ptr<Complete> ngCreateSnapshotSource(std::shared_ptr<core::Puppet> puppet) {
    static_assert(std::is_convertible_v<Complete*, Snapshot*>);
    auto result = std::allocate_shared<Complete>(core::d_runtime::SourceAllocator<Complete>{},
        SourceSnapshotConstructionTag{});
    result->Snapshot::initializeSnapshotOwned(puppet);
    return result;
}
} // namespace nicxlive::utils

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nicxlive::utils::Snapshot> {
    inline static constexpr char className[] = "nijilive.utils.snapshot.Snapshot";
    inline static constexpr char templateArgumentName[] = "(nijilive.utils.snapshot.Snapshot)";
};
template<> struct SourceTypeSpellings<::nicxlive::utils::Snapshot> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijilive.utils.snapshot.Snapshot)";
    inline static constexpr char unqualifiedArgument[] = "(Snapshot)";
    inline static constexpr char qualifiedElement[] = "nijilive.utils.snapshot.Snapshot";
    inline static constexpr char unqualifiedElement[] = "Snapshot";
};
// Explicit specialization names this real protected record without changing
// its access. It is a Source value type, not another identity-bearing Object.
template<> struct SourceTypeSpellings<::nicxlive::utils::Snapshot::PendingRequest> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijilive.utils.snapshot.Snapshot.PendingRequest)";
    inline static constexpr char unqualifiedArgument[] = "(PendingRequest)";
    inline static constexpr char qualifiedElement[] = "nijilive.utils.snapshot.Snapshot.PendingRequest";
    inline static constexpr char unqualifiedElement[] = "PendingRequest";
};
} // namespace nicxlive::core::d_runtime
