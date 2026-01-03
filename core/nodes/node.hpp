#pragma once

#include "common.hpp"
#include "../serde.hpp"

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
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

struct RenderScopeHint {
    enum class Kind { Root, Skip, Dynamic };
    Kind kind{Kind::Root};
    std::size_t token{static_cast<std::size_t>(-1)};

    static RenderScopeHint root() { return RenderScopeHint{Kind::Root, static_cast<std::size_t>(-1)}; }
    static RenderScopeHint skipHint() { return RenderScopeHint{Kind::Skip, static_cast<std::size_t>(-1)}; }
    static RenderScopeHint forDynamic(std::size_t t) { return RenderScopeHint{Kind::Dynamic, t}; }
};

class Node : public std::enable_shared_from_this<Node> {
public:
    struct FilterHook {
        int stage{0};
        using Func = std::function<std::tuple<std::vector<Vec2>, std::optional<Mat4>, bool>(
            std::shared_ptr<Node>, const std::vector<Vec2>&, const std::vector<Vec2>&, const Mat4*)>;
        Func func{};
    };

    uint32_t uuid{0};
    std::string name{"Unnamed Node"};
    Transform localTransform{};
    Transform offsetTransform{};
    float offsetSort{0.0f};
    float zsortRel{0.0f};
    bool enabled{true};
    bool lockToRoot{false};
    bool pinToMesh{false};
    bool allowRenderTasks{true};

    NodeTaskFlag taskFlags{NodeTaskFlag::None};
    std::optional<Mat4> overrideTransformMatrix{};
    std::optional<Transform> oneTimeTransform{};

    std::vector<std::weak_ptr<Node>> notifyListeners{};
    bool changed{false};
    bool changeDeferred{false};
    std::vector<std::pair<std::weak_ptr<Node>, NotifyReason>> changePool{};
    std::string nodePathCache{};

    std::weak_ptr<Node> parent{};
    std::vector<std::shared_ptr<Node>> children{};

    Mat4 cachedWorld{Mat4::identity()};
    bool recalcTransform{true};
    Transform globalTransform{};
    std::weak_ptr<::nicxlive::core::Puppet> puppet{};

    // Filters
    std::vector<FilterHook> preProcessFilters{};
    std::vector<FilterHook> postProcessFilters{};
    bool preProcessed{false};
    int postProcessed{-1};

    virtual ~Node() = default;

    // Core behaviour
    virtual bool renderEnabled() const;
    virtual const std::string& typeId() const;
    virtual float zSortBase() const;
    virtual float relZSort() const;
    virtual float zSortNoOffset() const;
    virtual float zSort() const;

    void zSort(float value);
    void setAbsZSort(float value);

    void setParent(const std::shared_ptr<Node>& p);
    std::shared_ptr<Node> parentPtr() const;
    void addChild(const std::shared_ptr<Node>& child);
    void clearChildren();
    std::size_t getIndexInParent() const;
    std::size_t getIndexInNode(const std::shared_ptr<Node>& n) const;
    const std::vector<std::shared_ptr<Node>>& childrenList() const { return children; }

    void requirePreProcessTask();
    void requireDynamicTask();
    void requirePostTask(std::size_t stage);
    void requireRenderBeginTask();
    void requireRenderTask();
    void requireRenderEndTask();

    void preProcess();
    void postProcess(int id = 0);

    Transform transform();
    Transform transform() const;
    Transform transformLocal();
    Transform transformLocal() const;
    Transform transformNoLock();
    Transform transformNoLock() const;

    void setRelativeTo(const std::shared_ptr<Node>& to);
    void setRelativeTo(const Mat4& to);
    static Vec3 getRelativePosition(const Mat4& m1, const Mat4& m2);
    static Vec3 getRelativePositionInv(const Mat4& m1, const Mat4& m2);

    std::string getNodePath();
    int depth() const;
    std::vector<std::shared_ptr<Node>>& childrenRef() { return children; }
    std::shared_ptr<Node> parentRef() const { return parent.lock(); }

    void insertInto(const std::shared_ptr<Node>& node, std::size_t offset);
    static constexpr std::size_t OFFSET_START = 0;
    static constexpr std::size_t OFFSET_END = static_cast<std::size_t>(-1);

    bool lockToRootValue() const { return lockToRoot; }
    void setLockToRoot(bool value);

    bool pinToMeshValue() const { return pinToMesh; }
    void setPinToMesh(bool value);

    const char* cName() const { return name.c_str(); }

    std::shared_ptr<::nicxlive::core::Puppet> puppetRef() const;
    void setPuppet(const std::shared_ptr<::nicxlive::core::Puppet>& p) { puppet = p; }

    virtual bool hasParam(const std::string& key) const;
    virtual float getDefaultValue(const std::string& key) const;
    virtual bool setValue(const std::string& key, float value);
    virtual float scaleValue(const std::string& key, float value, int axis, float scale) const;
    virtual float getValue(const std::string& key) const;
    bool getEnabled() const { return enabled; }
    void setEnabled(bool value);

    virtual void draw();
    virtual void drawOne() {}
    virtual void reconstruct();
    virtual void finalize();
    virtual void transformChanged();
    virtual void resetMask();
    virtual void notifyChange(const std::shared_ptr<Node>& target, NotifyReason reason = NotifyReason::Transformed);
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

    std::array<float, 4> getInitialBoundsSize() const;
    std::array<float, 4> getCombinedBounds(bool reupdate = false, bool countPuppet = false);
    Rect getCombinedBoundsRect(bool reupdate = false, bool countPuppet = false);
    void drawOrientation();
    void drawBounds();
    void setOneTimeTransform(const std::shared_ptr<Mat4>& transform);
    std::shared_ptr<Mat4> getOneTimeTransform() const { return oneTimeTransformPtr; }
    void reparent(const std::shared_ptr<Node>& parent, std::size_t offset, bool ignoreTransform = false);
    virtual bool setupChild(const std::shared_ptr<Node>& child);
    virtual bool releaseChild(const std::shared_ptr<Node>& child);
    virtual void setupSelf();
    virtual void releaseSelf();
    Mat4 getDynamicMatrix() const;
    virtual void clearCache() {}
    virtual void normalizeUV(void* /*data*/) {}
    void flushNotifyChange();
    bool canReparent(const std::shared_ptr<Node>& to) const;
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
    static bool inHasNodeType(const std::string& id);
    static void inRegisterNodeType(const std::string& id, const std::function<std::shared_ptr<Node>()>& factory);
    static void inAliasNodeType(const std::string& existingId, const std::string& aliasId);
    static std::shared_ptr<Node> inInstantiateNode(const std::string& id, const std::shared_ptr<Node>& parent = nullptr);
    virtual void serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive = true, SerializeNodeFlags flags = SerializeNodeFlags::All) const;
    virtual void serializeSelf(::nicxlive::core::serde::InochiSerializer& serializer) const;
    virtual void serializePartial(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive = true) const;
    virtual void serializePartial(::nicxlive::core::serde::InochiSerializer& serializer, SerializeNodeFlags flags, bool recursive = true) const;
    virtual ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data);
    virtual ::nicxlive::core::serde::SerdeException deserializeTransform(const ::nicxlive::core::serde::Fghj& data);

private:
    void setRelativeTo(const Mat4& to, float zsortBase);
    std::shared_ptr<Mat4> oneTimeTransformPtr{};
};

class TmpNode : public Node {
public:
    TmpNode() = default;
};

} // namespace nicxlive::core::nodes
