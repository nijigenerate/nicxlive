#pragma once

#include "common.hpp"
#include "../common/nullable_string.hpp"
#include "../common/shared_array.hpp"
#include "../resource.hpp"
#include "../compat/native_access_fault.hpp"
#include "../serde.hpp"
#include "../serde_json.hpp"
#include "../serde_reader.hpp"
#include "../render/render_pass.hpp"

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace nicxlive::core {
class Puppet;
class TaskScheduler;
class RenderContext;
enum class TaskOrder;
enum class TaskKind;
} // namespace nicxlive::core

namespace nicxlive::core::nodes {

namespace core = ::nicxlive::core;

enum class NodeTaskFlag : uint32_t {
    None = 0,
    PreProcess = 1 << 0,
    Dynamic = 1 << 1,
    Post0 = 1 << 2,
    Post1 = 1 << 3,
    Post2 = 1 << 4,
    RenderBegin = 1 << 5,
    Render = 1 << 6,
    RenderEnd = 1 << 7,
};

inline NodeTaskFlag operator|(NodeTaskFlag lhs, NodeTaskFlag rhs) {
    return static_cast<NodeTaskFlag>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline NodeTaskFlag operator&(NodeTaskFlag lhs, NodeTaskFlag rhs) {
    return static_cast<NodeTaskFlag>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

inline bool has_flag(NodeTaskFlag value, NodeTaskFlag test) {
    return static_cast<uint32_t>(value & test) != 0;
}

enum class SerializeNodeFlags : uint32_t {
    None = 0,
    Basics = 1 << 0,
    State = 1 << 1,
    Children = 1 << 2,
    Geometry = 1 << 3,
    Links = 1 << 4,
    All = Basics | State | Children | Geometry | Links,
};

inline SerializeNodeFlags operator|(SerializeNodeFlags lhs, SerializeNodeFlags rhs) {
    return static_cast<SerializeNodeFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline SerializeNodeFlags operator&(SerializeNodeFlags lhs, SerializeNodeFlags rhs) {
    return static_cast<SerializeNodeFlags>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

inline bool has_flag(SerializeNodeFlags value, SerializeNodeFlags test) {
    return static_cast<uint32_t>(value & test) != 0;
}

enum class NotifyReason {
    Initialized,
    Transformed,
    StructureChanged,
    AttributeChanged,
};

class Drawable;
class Node;

// A delegate descriptor retains its actual context and compares context/invoker identity.
class NodeNotifyListener {
private:
    template<class R, class C, class... A>
    static std::type_identity<C> methodClass(R (C::*)(A...));
    template<class R, class C, class... A>
    static std::type_identity<C> methodClass(R (C::*)(A...) const);
    template<class R, class C, class... A>
    static std::type_identity<C> methodClass(R (C::*)(A...) noexcept);
    template<class R, class C, class... A>
    static std::type_identity<C> methodClass(R (C::*)(A...) const noexcept);
    template<auto Method> using MethodClass = typename decltype(methodClass(Method))::type;
    template<auto Method>
    static void invokeMethod(void* context, const std::shared_ptr<Node>& target, NotifyReason reason) {
        std::invoke(Method, static_cast<MethodClass<Method>*>(context), target, reason);
    }
public:
    using Function = std::function<void(std::shared_ptr<Node>, NotifyReason)>;
    using Invoker = void (*)(void*, const std::shared_ptr<Node>&, NotifyReason);
    using FunctionPointer = void (*)(std::shared_ptr<Node>, NotifyReason);
    NodeNotifyListener() noexcept = default;
    NodeNotifyListener(std::nullptr_t) noexcept {}
    NodeNotifyListener(FunctionPointer function) noexcept : direct_(function) {}
    template<class Callable> requires (
        !std::is_same_v<std::remove_cvref_t<Callable>, NodeNotifyListener> &&
        std::is_constructible_v<Function, Callable>)
    NodeNotifyListener(Callable&& callable) {
        Function function(std::forward<Callable>(callable));
        if (function) {
            context_ = std::make_shared<Function>(std::move(function));
            invoke_ = [](void* context, const std::shared_ptr<Node>& target, NotifyReason reason) {
                (*static_cast<Function*>(context))(target, reason);
            };
        }
    }
    NodeNotifyListener(const NodeNotifyListener&) noexcept = default;
    NodeNotifyListener& operator=(const NodeNotifyListener& other) noexcept {
        if (this == &other) return *this;
        auto incomingContext = other.context_;
        const auto incomingInvoke = other.invoke_;
        const auto incomingDirect = other.direct_;
        context_.swap(incomingContext);
        invoke_ = incomingInvoke;
        direct_ = incomingDirect;
        return *this;
    }
    NodeNotifyListener(NodeNotifyListener&& other) noexcept
        : NodeNotifyListener(static_cast<const NodeNotifyListener&>(other)) {}
    NodeNotifyListener& operator=(NodeNotifyListener&& other) noexcept {
        return *this = static_cast<const NodeNotifyListener&>(other);
    }
    static NodeNotifyListener fromContext(std::shared_ptr<void> context, Invoker invoke) noexcept {
        NodeNotifyListener result;
        result.context_ = std::move(context);
        result.invoke_ = invoke;
        return result;
    }
    template<auto Method, class Context>
    static NodeNotifyListener fromMethod(const std::shared_ptr<Context>& context) noexcept {
        if (!context) return {};
        if constexpr (std::is_pointer_v<decltype(Method)> || std::is_member_pointer_v<decltype(Method)>) {
            if constexpr (Method == nullptr) return {};
        }
        if constexpr (requires { typename MethodClass<Method>; }) {
            std::shared_ptr<MethodClass<Method>> receiver = context;
            return fromContext(std::move(receiver), &invokeMethod<Method>);
        } else {
            // Preserve native extended callable intakes outside the Source method family.
            return fromContext(context, [](void* receiver, const std::shared_ptr<Node>& target, NotifyReason reason) {
                std::invoke(Method, static_cast<Context*>(receiver), target, reason);
            });
        }
    }
    static NodeNotifyListener forNode(const std::shared_ptr<Node>& listener);
    explicit operator bool() const noexcept { return invoke_ != nullptr || direct_ != nullptr; }
    bool operator==(const NodeNotifyListener& other) const noexcept {
        return context_.get() == other.context_.get() && invoke_ == other.invoke_ && direct_ == other.direct_;
    }
    void operator()(const std::shared_ptr<Node>& target, NotifyReason reason) const {
        const auto context = context_;
        const auto invoke = invoke_;
        const auto direct = direct_;
        const auto actualTarget = target;
        if (direct) { direct(actualTarget, reason); return; }
        if (!invoke) d_runtime::ngFailNullAccess(false);
        invoke(context.get(), actualTarget, reason);
    }
private:
    std::shared_ptr<void> context_;
    Invoker invoke_ = nullptr;
    FunctionPointer direct_ = nullptr;
};

using RenderScopeHint = ::nicxlive::core::RenderScopeHint;

class Node : public ::nicxlive::core::Resource, public std::enable_shared_from_this<Node> {
public:
    class MatrixHolder {
    public:
        explicit MatrixHolder(Mat4 value) : matrix(value) {}
        virtual ~MatrixHolder() = default;
        Mat4 matrix;
        // Existing native matrix readers consume the held value, not a copy of this class.
        operator const Mat4&() const noexcept { return matrix; }
    };
    struct FilterHook {
        int stage{0};
        std::uintptr_t tag{0};
        class Func {
            using Result = std::tuple<Vec2Array, Mat4*, bool>;
            using Function = std::function<Result(std::shared_ptr<Node>, Vec2Array, Vec2Array, Mat4*)>;
            using MethodInvoker = Result (*)(void*, std::shared_ptr<Node>, Vec2Array, Vec2Array, Mat4*);
            std::shared_ptr<Function> callable_;
            std::shared_ptr<void> methodContext_;
            MethodInvoker methodInvoker_ = nullptr;

            template<class R, class C, class... A> static std::type_identity<C> methodClass(R (C::*)(A...));
            template<class R, class C, class... A> static std::type_identity<C> methodClass(R (C::*)(A...) const);
            template<class R, class C, class... A> static std::type_identity<C> methodClass(R (C::*)(A...) noexcept);
            template<class R, class C, class... A>
            static std::type_identity<C> methodClass(R (C::*)(A...) const noexcept);
            template<auto Method> using MethodClass = typename decltype(methodClass(Method))::type;

            template<auto Method> static Result invokeMethod(void* context, std::shared_ptr<Node> node,
                    Vec2Array vertices, Vec2Array deformation, Mat4* matrix) {
                return std::invoke(Method, static_cast<MethodClass<Method>*>(context),
                    node, vertices, deformation, matrix);
            }
        public:
            Func() noexcept = default;
            Func(std::nullptr_t) noexcept {}
            Func(const Func&) noexcept = default;
            Func& operator=(const Func& other) noexcept {
                if (this == &other) return *this;
                auto callable = other.callable_;
                auto context = other.methodContext_;
                const auto invoke = other.methodInvoker_;
                callable_.swap(callable);
                methodContext_.swap(context);
                methodInvoker_ = invoke;
                return *this;
            }
            Func(Func&& other) noexcept : Func(static_cast<const Func&>(other)) {}
            Func& operator=(Func&& other) noexcept { return operator=(static_cast<const Func&>(other)); }
            template<class Callable> requires (
                !std::is_same_v<std::remove_cvref_t<Callable>, Func> &&
                std::is_constructible_v<Function, Callable>)
            Func(Callable&& callable) {
                Function function(std::forward<Callable>(callable));
                if (function) callable_ = std::make_shared<Function>(std::move(function));
            }
            template<auto Method, class Owner> requires (
                std::is_member_function_pointer_v<decltype(Method)> &&
                std::is_convertible_v<Owner*, MethodClass<Method>*> &&
                std::is_invocable_r_v<Result, decltype(Method), MethodClass<Method>*,
                    std::shared_ptr<Node>, Vec2Array, Vec2Array, Mat4*>)
            static Func fromMethod(std::shared_ptr<Owner> owner) {
                if constexpr (Method == nullptr) return {};
                else {
                    if (!owner) return {};
                    std::shared_ptr<MethodClass<Method>> context = owner;
                    Func result;
                    result.methodContext_ = context;
                    result.methodInvoker_ = &invokeMethod<Method>;
                    return result;
                }
            }
            explicit operator bool() const noexcept {
                return methodInvoker_ != nullptr || (callable_ && static_cast<bool>(*callable_));
            }
            friend bool operator==(const Func& left, const Func& right) noexcept {
                if (left.methodInvoker_ || right.methodInvoker_)
                    return left.methodInvoker_ == right.methodInvoker_
                        && left.methodContext_.get() == right.methodContext_.get();
                return left.callable_ == right.callable_;
            }
            friend bool operator!=(const Func& left, const Func& right) noexcept { return !(left == right); }
            template<class Context> Context* target() noexcept {
                return callable_ ? callable_->template target<Context>() : nullptr;
            }
            template<class Context> const Context* target() const noexcept {
                return callable_ ? callable_->template target<Context>() : nullptr;
            }
            Result operator()(std::shared_ptr<Node> node, Vec2Array vertices,
                    Vec2Array deformation, Mat4* matrix) const {
                const auto context = methodContext_;
                const auto invoke = methodInvoker_;
                if (invoke) return invoke(context.get(), node, vertices, deformation, matrix);
                const auto callable = callable_;
                if (!callable || !*callable) d_runtime::ngFailNullAccess(false);
                return (*callable)(node, vertices, deformation, matrix);
            }
        };
        Func func{};
    };
    struct DeformFilterResult {
        std::optional<Mat4> transform{};
        bool changed{false};
        // The actual first component of the Source filter tuple, applied after return.
        Vec2Array deformation{};
    };
    struct DeformFilterHook {
        int stage{0};
        std::uintptr_t tag{0};
        // One real callable context is retained by all copies of a Source delegate.
        class Func {
            using Function = std::function<DeformFilterResult(
                std::shared_ptr<Node>, const Vec2Array&, Vec2Array&, Mat4*)>;
            std::shared_ptr<Function> callable_;
        public:
            Func() noexcept = default;
            Func(std::nullptr_t) noexcept {}
            Func(const Func&) noexcept = default;
            Func& operator=(const Func&) noexcept = default;
            Func(Func&& other) noexcept : callable_(other.callable_) {}
            Func& operator=(Func&& other) noexcept {
                callable_ = other.callable_;
                return *this;
            }
            template<class Callable> requires (
                !std::is_same_v<std::remove_cvref_t<Callable>, Func> &&
                std::is_constructible_v<Function, Callable>)
            Func(Callable&& callable) {
                Function function(std::forward<Callable>(callable));
                if (function) callable_ = std::make_shared<Function>(std::move(function));
            }
            explicit operator bool() const noexcept { return callable_ && static_cast<bool>(*callable_); }
            template<class Context> Context* target() noexcept {
                return callable_ ? callable_->template target<Context>() : nullptr;
            }
            template<class Context> const Context* target() const noexcept {
                return callable_ ? callable_->template target<Context>() : nullptr;
            }
            DeformFilterResult operator()(std::shared_ptr<Node> node, const Vec2Array& vertices,
                    Vec2Array& deformation, const Mat4* matrix) const {
                const auto callable = callable_;
                if (!callable || !*callable) d_runtime::ngFailNullAccess(false);
                return (*callable)(node, vertices, deformation, const_cast<Mat4*>(matrix));
            }
        };
        Func func{};
    };

    uint32_t uuid{0};
    common::NullableString name = common::NullableString::fromStaticLiteral("Unnamed Node");
    Transform localTransform{};
    Transform offsetTransform{};
    float offsetSort{0.0f};
    float zsortRel{0.0f};
    bool enabled{true};
    bool lockToRoot{false};
    bool pinToMesh{false};
    bool allowRenderTasks{true};

    NodeTaskFlag taskFlags{NodeTaskFlag::None};
    std::shared_ptr<MatrixHolder> overrideTransformMatrix{};
    std::optional<Transform> oneTimeTransform{};

    using NotifyListener = NodeNotifyListener;
    common::SharedArray<NotifyListener> notifyListeners{};
    bool changed{false};
    bool changeDeferred{false};
    common::SharedArray<std::pair<std::shared_ptr<Node>, NotifyReason>> changePool{};
    common::NullableString nodePathCache{};

    std::weak_ptr<Node> parent{};
    using ChildArray = common::SharedArray<std::shared_ptr<Node>>;
    ChildArray children{};

    Mat4 cachedWorld{Mat4::identity()};
    bool recalcTransform{true};
    Transform globalTransform{};
    std::weak_ptr<::nicxlive::core::Puppet> puppet{};

    // Filters
    using FilterArray = common::SharedArray<FilterHook>;
    FilterArray preProcessFilters{};
    FilterArray postProcessFilters{};
    bool preProcessed{false};
    int postProcessed{-1};

    virtual ~Node() = default;

    // Source constructors attach a fully owned object before publishing its UUID.
    // Derived allocation paths call this before their source constructor body.
    void constructWithParent(uint32_t id, const std::shared_ptr<Node>& parent = nullptr);
    void constructWithParent(const std::shared_ptr<Node>& parent = nullptr);
    static std::shared_ptr<Node> create(const std::shared_ptr<Node>& parent = nullptr);
    template<class ParentProducer>
    static std::shared_ptr<Node> createSourceDeferred(ParentProducer&& makeParent) {
        auto node = allocateSourceReceiver();
        std::shared_ptr<Node> parent = std::invoke(std::forward<ParentProducer>(makeParent));
        node->constructWithParent(parent);
        return node;
    }
    static std::shared_ptr<Node> create(uint32_t id, const std::shared_ptr<Node>& parent = nullptr);
    void constructWithPuppet(const std::shared_ptr<::nicxlive::core::Puppet>& owner);
    static std::shared_ptr<Node> createForPuppet(const std::shared_ptr<::nicxlive::core::Puppet>& owner);

    // Core behaviour
    virtual bool renderEnabled() const;
    virtual common::NullableString typeId() const;
    virtual float zSortBase() const;
    virtual float relZSort() const;
    virtual float zSortNoOffset() const;
    virtual float zSort() const;


    void setParent(const std::shared_ptr<Node>& p);
    std::shared_ptr<Node> parentPtr() const;
    void addChild(const std::shared_ptr<Node>& child);
    void clearChildren();
    std::size_t getIndexInParent() const;
    std::size_t getIndexInNode(const std::shared_ptr<Node>& n) const;
    // Source final queries preserve the signed countUntil result and -1 sentinel.
    std::ptrdiff_t getIndexInParentSource();
    std::ptrdiff_t getIndexInNodeSource(std::shared_ptr<Node> node);
    ChildArray childrenList() const { return children; }

    void requirePreProcessTask();
    void requireDynamicTask();
    void requirePostTask(std::size_t stage);
    void requireRenderBeginTask();
    void requireRenderTask();
    void requireRenderEndTask();

    virtual void preProcess();
    virtual void postProcess(int id = 0);

    virtual Transform transform();
    virtual Transform transform() const;
    Transform transformLocal() const;
    virtual Transform transformNoLock();
    Transform transformNoLock() const;

    static Vec3 getRelativePosition(const Mat4& m1, const Mat4& m2);
    static Vec3 getRelativePositionInv(const Mat4& m1, const Mat4& m2);

    std::string getNodePath();
    d_runtime::RetainedString getNodePathRetained();
    int depth() const;
    ChildArray& childrenRef() { return children; }
    std::shared_ptr<Node> parentRef() const { return parent.lock(); }

    void insertInto(const std::shared_ptr<Node>& node, std::size_t offset);
    static constexpr std::size_t OFFSET_START = 0;
    static constexpr std::size_t OFFSET_END = static_cast<std::size_t>(-1);

    virtual bool& lockToRootValue() { return lockToRoot; }

    bool pinToMeshValue() const { return pinToMesh; }
    void setPinToMesh(bool value);

    std::uint32_t getUUID() const override { return uuid; }
    common::NullableString getName() const override { return name; }
    d_runtime::RetainedString toString() const override { return d_runtime::RetainedString(name); }
    virtual void setName(common::NullableString value) { name = std::move(value); }
    virtual const char* cName() const {
        cNameValue_ = getName();
        return cNameValue_.c_str();
    }

    std::shared_ptr<::nicxlive::core::Puppet> puppetRef() const;
    void setPuppet(const std::shared_ptr<::nicxlive::core::Puppet>& p) { puppet = p; }

    virtual bool hasParam(const std::string& key) const;
    virtual float getDefaultValue(const std::string& key) const;
    virtual bool setValue(const std::string& key, float value);
    virtual float scaleValue(const std::string& key, float value, int axis, float scale) const;
    virtual float getValue(const std::string& key) const;
    bool getEnabled() const { return const_cast<Node*>(this)->getEnabled(); }

    virtual void draw();
    virtual void drawOne() {}
    virtual void reconstruct();
    virtual void finalize();
    virtual void transformChanged();
    virtual void resetMask();
    virtual void notifyChange(const std::shared_ptr<Node>& target, NotifyReason reason = NotifyReason::Transformed);
    virtual void addNotifyListener(NotifyListener listener);
    virtual void removeNotifyListener(NotifyListener listener);
    void addNotifyListener(std::nullptr_t) { addNotifyListener(NotifyListener{}); }
    void removeNotifyListener(std::nullptr_t) { removeNotifyListener(NotifyListener{}); }
    virtual void addNotifyListener(const std::shared_ptr<Node>& listener);
    virtual void removeNotifyListener(const std::shared_ptr<Node>& listener);
    virtual void runBeginTask(core::RenderContext& ctx);
    virtual void runPreProcessTask(core::RenderContext& ctx);
    virtual void runDynamicTask(core::RenderContext& ctx);
    virtual void runPostTaskImpl(std::size_t priority, core::RenderContext& ctx);
    void runPostTask0();
    void runPostTask1();
    void runPostTask2();
    virtual void runFinalTask(core::RenderContext& ctx);
    virtual void runRenderTask(core::RenderContext& ctx);
    virtual void runRenderBeginTask(core::RenderContext& ctx);
    virtual void runRenderEndTask(core::RenderContext& ctx);
    virtual void registerRenderTasks(core::TaskScheduler& scheduler);
    virtual RenderScopeHint determineRenderScopeHint();

    virtual std::array<float, 4> getInitialBoundsSize();
    std::array<float, 4> getCombinedBounds(bool reupdate = false, bool countPuppet = false);
    Rect getCombinedBoundsRect(bool reupdate = false, bool countPuppet = false);
    virtual void drawBounds();
    std::shared_ptr<Mat4> getOneTimeTransform() const;
    virtual bool setupChild(const std::shared_ptr<Node>& child);
    virtual bool releaseChild(const std::shared_ptr<Node>& child);
    virtual void setupSelf();
    virtual void releaseSelf();
    Mat4 getDynamicMatrix() const;
    virtual void clearCache() {}
    virtual void normalizeUV(void* /*data*/) {}
    void flushNotifyChange();
    void forceSetUUID(uint32_t id) { uuid = id; }
    virtual void centralize();
    virtual void copyFrom(const Node& src, bool clone = false, bool deepCopy = true);
    virtual std::shared_ptr<Node> dup();
    virtual void build(bool force = false);
    virtual bool coverOthers() const;
    virtual bool mustPropagate() const;
    static uint32_t inCreateUUID();
    static void inUnloadUUID(uint32_t id);
    static void inClearUUIDs();
    static void inInitBuiltinNodeTypes();
    // Original package inInitNodes registers only these two base node types.
    static void inInitNodesSource();
    static bool inHasNodeType(const std::string& id);
    static void inRegisterNodeType(const std::string& id, const std::function<std::shared_ptr<Node>()>& factory);
    using ParentNodeFactory = std::function<std::shared_ptr<Node>(const std::shared_ptr<Node>&)>;
    static void inRegisterNodeType(const std::string& id, const ParentNodeFactory& factory);
    static void inAliasNodeType(const std::string& existingId, const std::string& aliasId);
    static std::shared_ptr<Node> inInstantiateNode(const std::string& id, const std::shared_ptr<Node>& parent = nullptr);
    // Typed IO preserves the original custom serializer dispatch.
    virtual void serializeSelfImpl(::nicxlive::core::serde::TypedJsonWriter& serializer, bool recursive = true, SerializeNodeFlags flags = SerializeNodeFlags::All) const;
    virtual void serializeSelf(::nicxlive::core::serde::TypedJsonWriter& serializer) const;
    void serialize(::nicxlive::core::serde::TypedJsonWriter& serializer) const;
    virtual void serializePartial(::nicxlive::core::serde::TypedJsonWriter& serializer, bool recursive = true) const;
    virtual void serializePartial(::nicxlive::core::serde::TypedJsonWriter& serializer, SerializeNodeFlags flags, bool recursive = true) const;
    virtual ::nicxlive::core::serde::ReadError deserializeFromFghj(::nicxlive::core::serde::TypedJsonView data);

    virtual void serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive = true, SerializeNodeFlags flags = SerializeNodeFlags::All) const;
    virtual void serializeSelf(::nicxlive::core::serde::InochiSerializer& serializer) const;
    virtual void serializePartial(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive = true) const;
    virtual void serializePartial(::nicxlive::core::serde::InochiSerializer& serializer, SerializeNodeFlags flags, bool recursive = true) const;
    virtual ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data);
    virtual ::nicxlive::core::serde::SerdeException deserializeTransform(const ::nicxlive::core::serde::Fghj& data);

    // Append the Source property setter after the existing virtual slots.
    virtual void setLockToRoot(bool value);

    // Nullable Source handle ingress retains the existing derived copy dispatch.
    virtual void copyFromSource(std::shared_ptr<Node> src, bool clone = false, bool deepCopy = true);

    static std::shared_ptr<Node> inInstantiateNodeRetained(d_runtime::RetainedString id,
        const std::shared_ptr<Node>& parent = nullptr);
    static bool inHasNodeTypeRetained(d_runtime::RetainedString id);
    static void inRegisterNodeTypeRetained(d_runtime::RetainedString id, const ParentNodeFactory& factory);
    static void inAliasNodeTypeRetained(d_runtime::RetainedString existingId, d_runtime::RetainedString aliasId);

    // Source parameter names retain their genuine immutable string descriptor.
    virtual bool hasParamSource(d_runtime::RetainedString sourceKey);
    virtual float getDefaultValueSource(d_runtime::RetainedString sourceKey);
    virtual bool setValueSource(d_runtime::RetainedString sourceKey, float value);
    virtual float scaleValueSource(d_runtime::RetainedString sourceKey, float value, int axis, float scale);
    virtual float getValueSource(d_runtime::RetainedString sourceKey);

    // The final Source parent property uses the actual insertion protocol.
    void setParentSource(std::shared_ptr<Node> parent);

    // Preserve Source virtual graph dispatch after all existing native slots.
    virtual void reparent(const std::shared_ptr<Node>& parent, std::size_t offset, bool ignoreTransform = false);
    virtual bool canReparent(const std::shared_ptr<Node>& to) const;
    virtual void setRelativeTo(const std::shared_ptr<Node>& to);
    virtual void setRelativeTo(const Mat4& to);
    virtual void zSort(float value);

    // Preserve Source local-transform dispatch after all existing virtual slots.
    virtual Transform transformLocal();
    virtual bool getEnabled() { return enabled; }
    virtual void setEnabled(bool value);

    // Source class handles preserve null and retain the actual scheduler object.
    virtual void registerRenderTasks(std::shared_ptr<core::TaskScheduler> scheduler);
    virtual void runPostTask0(core::RenderContext& ctx);
    virtual void runPostTask1(core::RenderContext& ctx);
    virtual void runPostTask2(core::RenderContext& ctx);

    // Source matrix methods remain mutable virtual calls after existing slots.
    virtual void setOneTimeTransform(const std::shared_ptr<Mat4>& transform);
    virtual std::shared_ptr<Mat4> getOneTimeTransform();
    virtual Mat4 getDynamicMatrix();

    // Source Z-sort accessors include the genuine mutable field reference.
    virtual float zSortBase();
    virtual float& relZSort();
    virtual float zSortNoOffset();
    virtual float zSort();
    virtual void setAbsZSort(float value);

    virtual void drawOrientation();

    // Source visibility predicate dispatches on the mutable logical receiver.
    virtual bool renderEnabled();

    // The actual Source CString result retains its fresh NUL-terminated buffer.
    virtual d_runtime::RetainedString cNameRetained();

private:
    static std::shared_ptr<Node> allocateSourceReceiver();
    mutable common::NullableString cNameValue_;
    void setRelativeTo(const Mat4& to, float zsortBase);
    std::shared_ptr<Mat4> oneTimeTransformPtr{};
};

class TmpNode : public Node {
public:
    TmpNode() = default;
    using Node::create;
    static std::shared_ptr<TmpNode> create() {
        ngRegisterModelResourceClassNames();
        return std::make_shared<TmpNode>();
    }
    static std::shared_ptr<TmpNode> create(const std::shared_ptr<Node>& parent) {
        const auto actualParent = parent;
        ngRegisterModelResourceClassNames();
        auto result = std::make_shared<TmpNode>();
        result->constructWithParent(actualParent);
        return result;
    }
    common::NullableString typeId() const override {
        return common::NullableString::fromStaticLiteral("Tmp");
    }
};

} // namespace nicxlive::core::nodes
