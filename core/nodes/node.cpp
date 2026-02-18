#include "node.hpp"

#include "../puppet.hpp"
#include "drawable.hpp"
#include "projectable.hpp"
#include "part.hpp"
#include "mask.hpp"
#include "composite.hpp"
#include "dynamic_composite.hpp"
#include "mesh_group.hpp"
#include "grid_deformer.hpp"
#include "path_deformer.hpp"
#include "driver.hpp"
#include "simple_physics_driver.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <unordered_map>

namespace nicxlive::core::nodes {

namespace {
std::vector<uint32_t> g_takenUUIDs{};
constexpr uint32_t kInvalidUUID = std::numeric_limits<uint32_t>::max();
std::mt19937& uuidRng() {
    static std::mt19937 rng{std::random_device{}()};
    return rng;
}

using NodeFactory = std::function<std::shared_ptr<Node>()>;
std::unordered_map<std::string, NodeFactory>& nodeFactories() {
    static std::unordered_map<std::string, NodeFactory> map{};
    return map;
}

void registerDefaultNodes() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    Node::inRegisterNodeType("Node", [] { return std::make_shared<Node>(); });
    Node::inRegisterNodeType("Tmp", [] { return std::make_shared<TmpNode>(); });
    Node::inRegisterNodeType("Part", [] { return std::make_shared<Part>(); });
    Node::inRegisterNodeType("Mask", [] { return std::make_shared<Mask>(); });
    Node::inRegisterNodeType("Composite", [] { return std::make_shared<Composite>(); });
    Node::inRegisterNodeType("DynamicComposite", [] { return std::make_shared<DynamicComposite>(); });
    Node::inRegisterNodeType("MeshGroup", [] { return std::make_shared<MeshGroup>(); });
    Node::inRegisterNodeType("GridDeformer", [] { return std::make_shared<GridDeformer>(); });
    Node::inRegisterNodeType("PathDeformer", [] { return std::make_shared<PathDeformer>(); });
    Node::inRegisterNodeType("Driver", [] { return std::shared_ptr<Driver>{}; });
    Node::inRegisterNodeType("SimplePhysics", [] { return std::make_shared<SimplePhysicsDriver>(); });
}

Transform combine(const Transform& a, const Transform& b) {
    Transform out = a;
    out.translation.x += b.translation.x;
    out.translation.y += b.translation.y;
    out.translation.z += b.translation.z;
    out.rotation.x += b.rotation.x;
    out.rotation.y += b.rotation.y;
    out.rotation.z += b.rotation.z;
    out.scale.x *= b.scale.x;
    out.scale.y *= b.scale.y;
    return out;
}

} // namespace

// Debug line storage (for drawOrientation / drawBounds parity)
static std::vector<DebugLine> g_debugLines{};

std::vector<DebugLine>& debugDrawBuffer() { return g_debugLines; }
void clearDebugDrawBuffer() { g_debugLines.clear(); }

bool Node::renderEnabled() const {
    if (auto p = parent.lock()) {
        return p->renderEnabled() && enabled;
    }
    return enabled;
}

const std::string& Node::typeId() const {
    static const std::string kType = "Node";
    return kType;
}

float Node::zSortBase() const {
    if (auto p = parent.lock()) {
        return p->zSort();
    }
    return 0.0f;
}

float Node::relZSort() const {
    return zsortRel;
}

float Node::zSortNoOffset() const {
    return zSortBase() + relZSort();
}

float Node::zSort() const {
    return zSortBase() + relZSort() + offsetSort;
}

void Node::zSort(float value) {
    zsortRel = value;
}

void Node::setAbsZSort(float value) {
    zsortRel = value - zSortBase() - offsetSort;
}

void Node::setParent(const std::shared_ptr<Node>& p) {
    parent = p;
}

std::shared_ptr<Node> Node::parentPtr() const {
    return parent.lock();
}

void Node::addChild(const std::shared_ptr<Node>& child) {
    if (!child) return;
    child->insertInto(shared_from_this(), OFFSET_END);
}

void Node::clearChildren() {
    for (auto& child : children) {
        if (child) {
            child->parent.reset();
        }
    }
    children.clear();
}

std::size_t Node::getIndexInParent() const {
    auto p = parent.lock();
    if (!p) return static_cast<std::size_t>(-1);
    auto it = std::find_if(p->children.begin(), p->children.end(), [this](const std::shared_ptr<Node>& n) {
        return n.get() == this;
    });
    if (it == p->children.end()) return static_cast<std::size_t>(-1);
    return static_cast<std::size_t>(std::distance(p->children.begin(), it));
}

std::size_t Node::getIndexInNode(const std::shared_ptr<Node>& n) const {
    if (!n) return static_cast<std::size_t>(-1);
    auto it = std::find_if(n->children.begin(), n->children.end(), [this](const std::shared_ptr<Node>& ptr) {
        return ptr.get() == this;
    });
    if (it == n->children.end()) return static_cast<std::size_t>(-1);
    return static_cast<std::size_t>(std::distance(n->children.begin(), it));
}

void Node::requirePreProcessTask() {
    taskFlags = taskFlags | NodeTaskFlag::PreProcess;
}

void Node::requireDynamicTask() {
    taskFlags = taskFlags | NodeTaskFlag::Dynamic;
}

void Node::requirePostTask(std::size_t stage) {
    switch (stage) {
    case 0: taskFlags = taskFlags | NodeTaskFlag::Post0; break;
    case 1: taskFlags = taskFlags | NodeTaskFlag::Post1; break;
    case 2: taskFlags = taskFlags | NodeTaskFlag::Post2; break;
    default: break;
    }
}

void Node::requireRenderBeginTask() {
    taskFlags = taskFlags | NodeTaskFlag::RenderBegin;
}

void Node::requireRenderTask() {
    taskFlags = taskFlags | NodeTaskFlag::Render;
}

void Node::requireRenderEndTask() {
    taskFlags = taskFlags | NodeTaskFlag::RenderEnd;
}

bool Node::inHasNodeType(const std::string& id) {
    registerDefaultNodes();
    return nodeFactories().find(id) != nodeFactories().end();
}

void Node::inRegisterNodeType(const std::string& id, const std::function<std::shared_ptr<Node>()>& factory) {
    nodeFactories()[id] = factory;
}

void Node::inAliasNodeType(const std::string& existingId, const std::string& aliasId) {
    registerDefaultNodes();
    auto it = nodeFactories().find(existingId);
    if (it != nodeFactories().end()) {
        nodeFactories()[aliasId] = it->second;
    }
}

std::shared_ptr<Node> Node::inInstantiateNode(const std::string& id, const std::shared_ptr<Node>& parent) {
    registerDefaultNodes();
    auto it = nodeFactories().find(id);
    std::shared_ptr<Node> node;
    if (it != nodeFactories().end()) {
        node = it->second();
    } else {
        node = std::make_shared<Node>();
    }
    if (parent && node) {
        parent->addChild(node);
    }
    return node;
}

std::shared_ptr<::nicxlive::core::Puppet> Node::puppetRef() const {
    if (auto p = parent.lock()) {
        if (auto pp = p->puppetRef()) return pp;
    }
    return puppet.lock();
}

void Node::preProcess() {
    if (preProcessed) return;
    preProcessed = true;
    for (const auto& hook : preProcessFilters) {
        Mat4 matrix = Mat4::identity();
        if (auto p = parent.lock()) {
            matrix = overrideTransformMatrix ? *overrideTransformMatrix : p->transform().toMat4();
        }
        std::vector<Vec2> localTrans{Vec2{localTransform.translation.x, localTransform.translation.y}};
        std::vector<Vec2> offsetTrans{Vec2{offsetTransform.translation.x, offsetTransform.translation.y}};
        auto result = hook.func(shared_from_this(), localTrans, offsetTrans, &matrix);
        const auto& newOffset = std::get<0>(result);
        const auto& newMat = std::get<1>(result);
        bool notify = std::get<2>(result);
        if (!newOffset.empty()) {
            offsetTransform.translation.x = newOffset[0].x;
            offsetTransform.translation.y = newOffset[0].y;
            transformChanged();
        }
        if (newMat.has_value()) {
            overrideTransformMatrix = newMat;
        }
        if (notify) {
            notifyChange(shared_from_this());
        }
    }
}

void Node::postProcess(int id) {
    if (postProcessed >= id) return;
    postProcessed = id;
    for (const auto& hook : postProcessFilters) {
        if (hook.stage != id) continue;
        Mat4 matrix = Mat4::identity();
        if (auto p = parent.lock()) {
            matrix = overrideTransformMatrix ? *overrideTransformMatrix : p->transform().toMat4();
        }
        std::vector<Vec2> localTrans{Vec2{localTransform.translation.x, localTransform.translation.y}};
        std::vector<Vec2> offsetTrans{Vec2{offsetTransform.translation.x, offsetTransform.translation.y}};
        auto result = hook.func(shared_from_this(), localTrans, offsetTrans, &matrix);
        const auto& newOffset = std::get<0>(result);
        const auto& newMat = std::get<1>(result);
        bool notify = std::get<2>(result);
        if (!newOffset.empty()) {
            offsetTransform.translation.x = newOffset[0].x;
            offsetTransform.translation.y = newOffset[0].y;
            transformChanged();
            overrideTransformMatrix = transform().toMat4();
        }
        if (newMat.has_value()) {
            overrideTransformMatrix = newMat;
        }
        if (notify) {
            notifyChange(shared_from_this());
        }
    }
}

Transform Node::transform() {
    if (recalcTransform) {
        localTransform.update();
        offsetTransform.update();
        Transform combined = localTransform.calcOffset(offsetTransform);
        if (!lockToRoot) {
            if (auto p = parent.lock()) {
                combined = combine(combined, p->transform());
            }
        } else {
            if (auto pup = puppetRef()) {
                if (auto root = pup->root) {
                    combined = combine(combined, root->localTransform);
                } else {
                    combined = combine(combined, pup->transform);
                }
            }
        }
        // overrideTransformMatrix を優先
        if (overrideTransformMatrix) {
            cachedWorld = *overrideTransformMatrix;
            globalTransform.translation.x = cachedWorld[0][3];
            globalTransform.translation.y = cachedWorld[1][3];
            globalTransform.translation.z = cachedWorld[2][3];
        } else {
            // one-time transformを適用
            Mat4 mat = combined.toMat4();
            if (oneTimeTransformPtr) {
                mat = Mat4::multiply(mat, *oneTimeTransformPtr);
            }
            cachedWorld = mat;
            globalTransform = combined;
            globalTransform.translation.x = cachedWorld[0][3];
            globalTransform.translation.y = cachedWorld[1][3];
            globalTransform.translation.z = cachedWorld[2][3];
        }
        recalcTransform = false;
    }
    return globalTransform;
}

Transform Node::transform() const {
    return const_cast<Node*>(this)->transform();
}

Transform Node::transformLocal() {
    localTransform.update();
    return localTransform.calcOffset(offsetTransform);
}

Transform Node::transformLocal() const {
    return const_cast<Node*>(this)->transformLocal();
}

Transform Node::transformNoLock() {
    localTransform.update();
    Transform combined = localTransform;
    if (auto p = parent.lock()) {
        combined = combine(combined, p->transform());
    }
    return combined;
}

Transform Node::transformNoLock() const {
    return const_cast<Node*>(this)->transformNoLock();
}

void Node::setRelativeTo(const std::shared_ptr<Node>& to) {
    if (!to) return;
    setRelativeTo(to->transformNoLock().toMat4());
    zSort(zSortNoOffset() - to->zSortNoOffset());
}

void Node::setRelativeTo(const Mat4& to) {
    auto rel = getRelativePosition(to, transformNoLock().toMat4());
    localTransform.translation = rel;
    localTransform.update();
}

void Node::setRelativeTo(const Mat4& to, float zsortBase) {
    setRelativeTo(to);
    setAbsZSort(zsortBase);
}

Vec3 Node::getRelativePosition(const Mat4& m1, const Mat4& m2) {
    Mat4 inv = Mat4::inverse(m1);
    Mat4 cm = Mat4::multiply(inv, m2);
    return Vec3{cm[0][3], cm[1][3], cm[2][3]};
}

Vec3 Node::getRelativePositionInv(const Mat4& m1, const Mat4& m2) {
    Mat4 cm = Mat4::multiply(m2, Mat4::inverse(m1));
    return Vec3{cm[0][3], cm[1][3], cm[2][3]};
}

std::string Node::getNodePath() {
    if (!nodePathCache.empty()) return nodePathCache;
    std::vector<std::string> segments;
    auto current = shared_from_this();
    while (current) {
        segments.push_back(current->name);
        current = current->parent.lock();
    }
    std::reverse(segments.begin(), segments.end());
    std::string path;
    for (const auto& seg : segments) {
        path += "/";
        path += seg;
    }
    nodePathCache = path.empty() ? "/" : path;
    return nodePathCache;
}

int Node::depth() const {
    int d = 0;
    auto current = parent.lock();
    while (current) {
        ++d;
        current = current->parent.lock();
    }
    return d;
}

void Node::insertInto(const std::shared_ptr<Node>& node, std::size_t offset) {
    nodePathCache.clear();
    if (auto p = parent.lock()) {
        auto& siblings = p->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), shared_from_this()), siblings.end());
    }

    if (!node) {
        parent.reset();
        return;
    }

    parent = node;
    auto& dst = node->children;
    if (offset == OFFSET_START) {
        dst.insert(dst.begin(), shared_from_this());
    } else if (offset == OFFSET_END || offset >= dst.size()) {
        dst.push_back(shared_from_this());
    } else {
        dst.insert(dst.begin() + static_cast<std::ptrdiff_t>(offset), shared_from_this());
    }
    if (auto pup = puppetRef()) {
        pup->rescanNodes();
    }
}

void Node::setLockToRoot(bool value) {
    if (value == lockToRoot) return;
    if (value && !lockToRoot) {
        localTransform.translation = transformNoLock().translation;
    } else if (!value && lockToRoot) {
        auto p = parent.lock();
        if (p) {
            localTransform.translation.x -= p->transformNoLock().translation.x;
            localTransform.translation.y -= p->transformNoLock().translation.y;
            localTransform.translation.z -= p->transformNoLock().translation.z;
        }
    }
    lockToRoot = value;
}

void Node::setPinToMesh(bool value) {
    bool changedFlag = value != pinToMesh;
    pinToMesh = value;
    if (changedFlag) {
        if (auto p = parent.lock()) {
            if (pinToMesh) {
                p->setupChild(shared_from_this());
            } else {
                p->releaseChild(shared_from_this());
            }
        }
    }
}

bool Node::hasParam(const std::string& key) const {
    return key == "zSort" || key == "transform.t.x" || key == "transform.t.y" || key == "transform.t.z"
        || key == "transform.r.x" || key == "transform.r.y" || key == "transform.r.z"
        || key == "transform.s.x" || key == "transform.s.y";
}

float Node::getDefaultValue(const std::string& key) const {
    if (key == "transform.s.x" || key == "transform.s.y") return 1.0f;
    if (hasParam(key)) return 0.0f;
    return 0.0f;
}

bool Node::setValue(const std::string& key, float value) {
    if (!std::isfinite(value)) return false;
    if (key == "zSort") {
        if (!std::isfinite(offsetSort)) offsetSort = 0.0f;
        offsetSort += value;
        return true;
    }
    if (key == "transform.t.x") {
        if (!std::isfinite(offsetTransform.translation.x)) offsetTransform.translation.x = 0.0f;
        offsetTransform.translation.x += value;
        transformChanged();
        return true;
    }
    if (key == "transform.t.y") {
        if (!std::isfinite(offsetTransform.translation.y)) offsetTransform.translation.y = 0.0f;
        offsetTransform.translation.y += value;
        transformChanged();
        return true;
    }
    if (key == "transform.t.z") {
        if (!std::isfinite(offsetTransform.translation.z)) offsetTransform.translation.z = 0.0f;
        offsetTransform.translation.z += value;
        transformChanged();
        return true;
    }
    if (key == "transform.r.x") {
        if (!std::isfinite(offsetTransform.rotation.x)) offsetTransform.rotation.x = 0.0f;
        offsetTransform.rotation.x += value;
        transformChanged();
        return true;
    }
    if (key == "transform.r.y") {
        if (!std::isfinite(offsetTransform.rotation.y)) offsetTransform.rotation.y = 0.0f;
        offsetTransform.rotation.y += value;
        transformChanged();
        return true;
    }
    if (key == "transform.r.z") {
        if (!std::isfinite(offsetTransform.rotation.z)) offsetTransform.rotation.z = 0.0f;
        offsetTransform.rotation.z += value;
        transformChanged();
        return true;
    }
    if (key == "transform.s.x") {
        if (offsetTransform.scale.x == 0.0f || !std::isfinite(offsetTransform.scale.x)) {
            offsetTransform.scale.x = 1.0f;
        }
        offsetTransform.scale.x *= value;
        transformChanged();
        return true;
    }
    if (key == "transform.s.y") {
        if (offsetTransform.scale.y == 0.0f || !std::isfinite(offsetTransform.scale.y)) {
            offsetTransform.scale.y = 1.0f;
        }
        offsetTransform.scale.y *= value;
        transformChanged();
        return true;
    }
    return false;
}

float Node::scaleValue(const std::string& key, float value, int axis, float scale) const {
    if (axis == -1) return value * scale;
    float newVal = std::abs(scale) * value;
    if (key == "transform.r.z") {
        newVal = scale * value;
    } else if (key == "transform.r.y" || key == "transform.t.x") {
        if (axis == 0) newVal = scale * value;
    } else if (key == "transform.r.x" || key == "transform.t.y") {
        if (axis == 1) newVal = scale * value;
    }
    return newVal;
}

float Node::getValue(const std::string& key) const {
    if (key == "zSort") return offsetSort;
    if (key == "transform.t.x") return offsetTransform.translation.x;
    if (key == "transform.t.y") return offsetTransform.translation.y;
    if (key == "transform.t.z") return offsetTransform.translation.z;
    if (key == "transform.r.x") return offsetTransform.rotation.x;
    if (key == "transform.r.y") return offsetTransform.rotation.y;
    if (key == "transform.r.z") return offsetTransform.rotation.z;
    if (key == "transform.s.x") return offsetTransform.scale.x;
    if (key == "transform.s.y") return offsetTransform.scale.y;
    return 0.0f;
}

void Node::setEnabled(bool value) {
    bool changedFlag = enabled != value;
    enabled = value;
    if (changedFlag) {
        notifyChange(shared_from_this(), NotifyReason::AttributeChanged);
    }
}

void Node::draw() {
    if (!renderEnabled()) return;
    for (auto& child : children) {
        if (child) child->draw();
    }
}

void Node::reconstruct() {
    auto copy = children;
    for (auto& child : copy) {
        if (child) child->reconstruct();
    }
}

void Node::finalize() {
    for (auto& child : children) {
        if (child) child->finalize();
    }
}

void Node::transformChanged() {
    recalcTransform = true;
    for (auto& child : children) {
        if (child) child->transformChanged();
    }
}

void Node::resetMask() {
    if (auto p = parent.lock()) {
        p->resetMask();
    }
}

void Node::notifyChange(const std::shared_ptr<Node>& target, NotifyReason reason) {
    if (!target) return;
    if (target.get() == this) changed = true;
    if (auto pup = puppetRef()) {
        pup->recordNodeChange(reason);
    }
    if (changeDeferred) {
        changePool.emplace_back(target, reason);
        return;
    }
    if (auto p = parent.lock()) {
        p->notifyChange(target, reason);
    }
    for (auto& weak : notifyListeners) {
        if (auto listener = weak.lock()) {
            listener->notifyChange(target, reason);
        }
    }
}

void Node::addNotifyListener(const std::shared_ptr<Node>& listener) {
    if (!listener) return;
    auto it = std::find_if(notifyListeners.begin(), notifyListeners.end(), [&](const std::weak_ptr<Node>& w) {
        return !w.expired() && w.lock().get() == listener.get();
    });
    if (it == notifyListeners.end()) {
        notifyListeners.push_back(listener);
    }
}

void Node::removeNotifyListener(const std::shared_ptr<Node>& listener) {
    if (!listener) return;
    notifyListeners.erase(
        std::remove_if(notifyListeners.begin(), notifyListeners.end(), [&](const std::weak_ptr<Node>& w) {
            auto l = w.lock();
            return !l || l.get() == listener.get();
        }),
        notifyListeners.end());
}

void Node::runBeginTask(core::RenderContext&) {
    preProcessed = false;
    postProcessed = -1;
    changed = false;
    changeDeferred = true;
    changePool.clear();
    offsetSort = 0.0f;
    offsetTransform.clear();
    overrideTransformMatrix.reset();
}

void Node::runPreProcessTask(core::RenderContext&) {
    preProcess();
}

void Node::runDynamicTask(core::RenderContext&) {
    // base: no-op (D 同様)
}

void Node::runPostTaskImpl(std::size_t priority, core::RenderContext&) {
    postProcess(static_cast<int>(priority));
}

void Node::runPostTask0() { core::RenderContext dummy{}; runPostTaskImpl(0, dummy); }
void Node::runPostTask1() { core::RenderContext dummy{}; runPostTaskImpl(1, dummy); }
void Node::runPostTask2() { core::RenderContext dummy{}; runPostTaskImpl(2, dummy); }

void Node::runFinalTask(core::RenderContext&) {
    postProcess(-1);
    flushNotifyChange();
}

void Node::runRenderTask(core::RenderContext&) {}
void Node::runRenderBeginTask(core::RenderContext&) {}
void Node::runRenderEndTask(core::RenderContext&) {}

void Node::registerRenderTasks(core::TaskScheduler& scheduler) {
    if (!allowRenderTasks) return;
    scheduler.addTask(core::TaskOrder::Init, core::TaskKind::Init, [this](core::RenderContext& ctx) { runBeginTask(ctx); });

    auto hasFlag = [&](NodeTaskFlag flag) { return has_flag(taskFlags, flag); };
    bool needPreProcess = (!preProcessFilters.empty()) || hasFlag(NodeTaskFlag::PreProcess);
    bool needDynamic = hasFlag(NodeTaskFlag::Dynamic);
    bool needPost[3]{false, false, false};
    for (const auto& hook : postProcessFilters) {
        if (hook.stage >= 0 && hook.stage < 3) {
            needPost[hook.stage] = true;
        }
    }
    if (hasFlag(NodeTaskFlag::Post0)) needPost[0] = true;
    if (hasFlag(NodeTaskFlag::Post1)) needPost[1] = true;
    if (hasFlag(NodeTaskFlag::Post2)) needPost[2] = true;
    bool needRenderBegin = hasFlag(NodeTaskFlag::RenderBegin);
    bool needRender = hasFlag(NodeTaskFlag::Render);
    bool needRenderEnd = hasFlag(NodeTaskFlag::RenderEnd);

    if (needPreProcess) {
        scheduler.addTask(core::TaskOrder::PreProcess, core::TaskKind::PreProcess, [this](core::RenderContext& ctx) { runPreProcessTask(ctx); });
    }
    if (needDynamic) {
        scheduler.addTask(core::TaskOrder::Dynamic, core::TaskKind::Dynamic, [this](core::RenderContext& ctx) { runDynamicTask(ctx); });
    }
    if (needPost[0]) scheduler.addTask(core::TaskOrder::Post0, core::TaskKind::PostProcess, [this](core::RenderContext& ctx) { runPostTaskImpl(0, ctx); });
    if (needPost[1]) scheduler.addTask(core::TaskOrder::Post1, core::TaskKind::PostProcess, [this](core::RenderContext& ctx) { runPostTaskImpl(1, ctx); });
    if (needPost[2]) scheduler.addTask(core::TaskOrder::Post2, core::TaskKind::PostProcess, [this](core::RenderContext& ctx) { runPostTaskImpl(2, ctx); });
    if (needRenderBegin) scheduler.addTask(core::TaskOrder::RenderBegin, core::TaskKind::Render, [this](core::RenderContext& ctx) { runRenderBeginTask(ctx); });
    if (needRender) scheduler.addTask(core::TaskOrder::Render, core::TaskKind::Render, [this](core::RenderContext& ctx) { runRenderTask(ctx); });
    scheduler.addTask(core::TaskOrder::Final, core::TaskKind::Finalize, [this](core::RenderContext& ctx) { runFinalTask(ctx); });

    auto orderedChildren = children;
    std::sort(orderedChildren.begin(), orderedChildren.end(), [](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
        if (!a && !b) return false;
        if (!a) return false;
        if (!b) return true;
        const float az = a->zSort();
        const float bz = b->zSort();
        if (az == bz) return a->uuid < b->uuid;
        return az > bz;
    });
    for (auto& child : orderedChildren) {
        if (child) child->registerRenderTasks(scheduler);
    }
    if (needRenderEnd) scheduler.addTask(core::TaskOrder::RenderEnd, core::TaskKind::Render, [this](core::RenderContext& ctx) { runRenderEndTask(ctx); });
}

RenderScopeHint Node::determineRenderScopeHint() {
    auto current = parent.lock();
    while (current) {
        if (auto proj = std::dynamic_pointer_cast<Projectable>(current)) {
            if (proj->dynamicScopeActive) {
                return RenderScopeHint::forDynamic(proj->dynamicScopeToken);
            }
            if (proj->reuseCachedTextureThisFrame) {
                return RenderScopeHint::skipHint();
            }
            return RenderScopeHint::skipHint();
        }
        current = current->parent.lock();
    }
    return RenderScopeHint::root();
}

std::array<float, 4> Node::getInitialBoundsSize() const {
    auto tr = const_cast<Node*>(this)->transform();
    return {tr.translation.x, tr.translation.y, tr.translation.x, tr.translation.y};
}

std::array<float, 4> Node::getCombinedBounds(bool reupdate, bool countPuppet) {
    auto combined = getInitialBoundsSize();
    if (auto drawable = std::dynamic_pointer_cast<Drawable>(shared_from_this())) {
        if (reupdate) drawable->updateBounds();
        if (drawable->bounds) {
            auto b = *drawable->bounds;
            combined = {b[0], b[1], b[2], b[3]};
        }
    }
    for (auto& child : children) {
        if (!child) continue;
        auto cb = child->getCombinedBounds(reupdate, false);
        combined[0] = std::min(combined[0], cb[0]);
        combined[1] = std::min(combined[1], cb[1]);
        combined[2] = std::max(combined[2], cb[2]);
        combined[3] = std::max(combined[3], cb[3]);
    }
    // Puppet transformを適用する場合
    if (countPuppet) {
        if (auto pup = puppetRef()) {
            auto mat = pup->transform.toMat4();
            auto apply = [&](float x, float y) {
                Vec3 v{x, y, 0.0f};
                auto tv = mat.transformPoint(v);
                return std::array<float, 2>{tv.x, tv.y};
            };
            auto p1 = apply(combined[0], combined[1]);
            auto p2 = apply(combined[2], combined[3]);
            combined[0] = std::min(p1[0], p2[0]);
            combined[1] = std::min(p1[1], p2[1]);
            combined[2] = std::max(p1[0], p2[0]);
            combined[3] = std::max(p1[1], p2[1]);
        }
    }
    return combined;
}

Rect Node::getCombinedBoundsRect(bool reupdate, bool countPuppet) {
    auto b = getCombinedBounds(reupdate, countPuppet);
    return Rect{b[0], b[1], b[2] - b[0], b[3] - b[1]};
}

void Node::drawOrientation() {
    // D 版同様にデバッグラインをバッファへ記録（描画は呼び出し側/バックエンドで処理）
    DebugLine lines[3]{
        {Vec3{0, 0, 0}, Vec3{32, 0, 0}, Vec4{1, 0, 0, 0.7f}},
        {Vec3{0, 0, 0}, Vec3{0, -32, 0}, Vec4{0, 1, 0, 0.7f}},
        {Vec3{0, 0, 0}, Vec3{0, 0, -32}, Vec4{0, 0, 1, 0.7f}},
    };
    Mat4 m = transform().toMat4();
    auto& buf = debugDrawBuffer();
    for (const auto& l : lines) {
        DebugLine world{
            m.transformPoint(l.a),
            m.transformPoint(l.b),
            l.color,
        };
        buf.push_back(world);
    }
}

void Node::drawBounds() {
    auto b = getCombinedBounds();
    float width = b[2] - b[0];
    float height = b[3] - b[1];
    DebugLine lines[4]{
        {Vec3{b[0], b[1], 0}, Vec3{b[0] + width, b[1], 0}, Vec4{0.5f, 0.5f, 0.5f, 1.0f}},
        {Vec3{b[0] + width, b[1], 0}, Vec3{b[0] + width, b[1] + height, 0}, Vec4{0.5f, 0.5f, 0.5f, 1.0f}},
        {Vec3{b[0] + width, b[1] + height, 0}, Vec3{b[0], b[1] + height, 0}, Vec4{0.5f, 0.5f, 0.5f, 1.0f}},
        {Vec3{b[0], b[1] + height, 0}, Vec3{b[0], b[1], 0}, Vec4{0.5f, 0.5f, 0.5f, 1.0f}},
    };
    auto& buf = debugDrawBuffer();
    Mat4 m = transform().toMat4();
    for (const auto& l : lines) {
        DebugLine world{
            m.transformPoint(l.a),
            m.transformPoint(l.b),
            l.color,
        };
        buf.push_back(world);
    }
}

void Node::setOneTimeTransform(const std::shared_ptr<Mat4>& transform) {
    oneTimeTransformPtr = transform;
    for (auto& c : children) {
        if (c) c->setOneTimeTransform(transform);
    }
}

void Node::centralize() {
    for (auto& child : children) {
        if (child) child->centralize();
    }

    std::array<float, 4> bounds{};
    std::vector<Vec3> childTranslations;
    if (!children.empty()) {
        bool initialized = false;
        for (auto& child : children) {
            if (!child) {
                childTranslations.push_back(Vec3{});
                continue;
            }
            auto cbounds = child->getCombinedBounds();
            if (!initialized) {
                bounds = cbounds;
                initialized = true;
            } else {
                bounds[0] = std::min(bounds[0], cbounds[0]);
                bounds[1] = std::min(bounds[1], cbounds[1]);
                bounds[2] = std::max(bounds[2], cbounds[2]);
                bounds[3] = std::max(bounds[3], cbounds[3]);
            }
            auto translated = child->transform().toMat4().transformPoint(Vec3{0, 0, 0});
            childTranslations.push_back(translated);
        }
        if (!initialized) {
            auto tr = transform();
            bounds = {tr.translation.x, tr.translation.y, tr.translation.x, tr.translation.y};
        }
    } else {
        auto tr = transform();
        bounds = {tr.translation.x, tr.translation.y, tr.translation.x, tr.translation.y};
    }

    Vec2 center{(bounds[0] + bounds[2]) * 0.5f, (bounds[1] + bounds[3]) * 0.5f};
    if (auto p = parent.lock()) {
        Mat4 inv = Mat4::inverse(p->transform().toMat4());
        auto cent = inv.transformPoint(Vec3{center.x, center.y, 0.0f});
        center.x = cent.x;
        center.y = cent.y;
    }
    localTransform.translation.x = center.x;
    localTransform.translation.y = center.y;
    transformChanged();

    Mat4 invSelf = Mat4::inverse(transform().toMat4());
    for (std::size_t i = 0; i < children.size() && i < childTranslations.size(); ++i) {
        auto& child = children[i];
        if (!child) continue;
        auto translated = invSelf.transformPoint(childTranslations[i]);
        child->localTransform.translation = translated;
        child->transformChanged();
    }
}

void Node::reparent(const std::shared_ptr<Node>& newParent, std::size_t offset, bool ignoreTransform) {
    if (newParent && !canReparent(newParent)) {
        return;
    }
    releaseSelf();
    auto current = parent.lock();
    while (current) {
        if (!current->releaseChild(shared_from_this())) break;
        current = current->parent.lock();
    }
    if (newParent && !ignoreTransform) {
        setRelativeTo(newParent);
    }
    insertInto(newParent, offset);
    if (newParent) {
        auto p = newParent;
        while (p) {
            if (!p->setupChild(shared_from_this())) break;
            p = p->parent.lock();
        }
    }
    setupSelf();
}

void Node::copyFrom(const Node& src, bool clone, bool deepCopy) {
    name = src.name + (clone ? "" : "'");
    enabled = src.enabled;
    zsortRel = src.zsortRel;
    lockToRoot = src.lockToRoot;
    nodePathCache = src.nodePathCache;
    changed = src.changed;
    offsetSort = src.offsetSort;
    offsetTransform = src.offsetTransform;
    pinToMesh = src.pinToMesh;
    allowRenderTasks = src.allowRenderTasks;
    overrideTransformMatrix = src.overrideTransformMatrix;
    oneTimeTransformPtr = src.oneTimeTransformPtr;
    taskFlags = src.taskFlags;
    preProcessFilters = src.preProcessFilters;
    postProcessFilters = src.postProcessFilters;

    if (clone) {
        uuid = src.uuid;
        puppet = src.puppet;
        localTransform = src.localTransform;
    } else {
        uuid = inCreateUUID();
        localTransform = src.transform();
    }
    globalTransform = src.globalTransform;
    cachedWorld = src.cachedWorld;

    finalize();

    if (deepCopy) {
        children.clear();
        for (const auto& child : src.children) {
            if (!child) continue;
            auto cloned = Node::inInstantiateNode(child->typeId());
            if (!cloned) continue;
            cloned->copyFrom(*child, clone, deepCopy);
            cloned->reparent(shared_from_this(), OFFSET_END, true);
            cloned->localTransform = child->localTransform;
        }
    }
}

std::shared_ptr<Node> Node::dup() {
    auto node = Node::inInstantiateNode(typeId());
    if (node) {
        node->copyFrom(*this, false, true);
    }
    return node;
}

bool Node::setupChild(const std::shared_ptr<Node>&) {
    // D 実装同様デフォルトで処理を止める
    return false;
}

bool Node::releaseChild(const std::shared_ptr<Node>&) {
    // D 実装同様デフォルトで処理を止める
    return false;
}

void Node::setupSelf() {
    // デフォルトは何もしない（D と同様）
}

void Node::releaseSelf() {
    // デフォルトは何もしない（D と同様）
}

Mat4 Node::getDynamicMatrix() const {
    if (overrideTransformMatrix) {
        return *overrideTransformMatrix;
    }
    return transform().toMat4();
}

void Node::build(bool force) {
    (void)force;
    for (auto& child : children) {
        if (child) child->build(force);
    }
}

bool Node::coverOthers() const {
    return false;
}

bool Node::mustPropagate() const {
    return false;
}

uint32_t Node::inCreateUUID() {
    std::uniform_int_distribution<uint32_t> dist(std::numeric_limits<uint32_t>::min(), kInvalidUUID - 1);
    uint32_t id = dist(uuidRng());
    while (std::find(g_takenUUIDs.begin(), g_takenUUIDs.end(), id) != g_takenUUIDs.end()) {
        id = dist(uuidRng());
    }
    g_takenUUIDs.push_back(id);
    return id;
}

void Node::inUnloadUUID(uint32_t id) {
    auto it = std::find(g_takenUUIDs.begin(), g_takenUUIDs.end(), id);
    if (it != g_takenUUIDs.end()) {
        g_takenUUIDs.erase(it);
    }
}

void Node::inClearUUIDs() {
    g_takenUUIDs.clear();
}

void Node::flushNotifyChange() {
    changeDeferred = false;
    for (auto& rec : changePool) {
        if (auto tgt = rec.first.lock()) {
            notifyChange(tgt, rec.second);
        }
    }
    changePool.clear();
}

bool Node::canReparent(const std::shared_ptr<Node>& to) const {
    auto tmp = to;
    while (tmp) {
        if (tmp.get() == this) return false;
        tmp = tmp->parentPtr();
    }
    return true;
}

::nicxlive::core::serde::SerdeException Node::deserializeTransform(const ::nicxlive::core::serde::Fghj& data) {
    try {
        if (auto tx = data.get_optional<float>("x")) localTransform.translation.x = *tx;
        if (auto ty = data.get_optional<float>("y")) localTransform.translation.y = *ty;
        if (auto tz = data.get_optional<float>("z")) localTransform.translation.z = *tz;
        if (auto rx = data.get_optional<float>("rx")) localTransform.rotation.x = *rx;
        if (auto ry = data.get_optional<float>("ry")) localTransform.rotation.y = *ry;
        if (auto rz = data.get_optional<float>("rz")) localTransform.rotation.z = *rz;
        if (auto sx = data.get_optional<float>("sx")) localTransform.scale.x = *sx;
        if (auto sy = data.get_optional<float>("sy")) localTransform.scale.y = *sy;
        if (auto ot = data.get_child_optional("offsetTransform")) {
            if (auto tx = ot->get_optional<float>("x")) offsetTransform.translation.x = *tx;
            if (auto ty = ot->get_optional<float>("y")) offsetTransform.translation.y = *ty;
            if (auto tz = ot->get_optional<float>("z")) offsetTransform.translation.z = *tz;
            if (auto rx = ot->get_optional<float>("rx")) offsetTransform.rotation.x = *rx;
            if (auto ry = ot->get_optional<float>("ry")) offsetTransform.rotation.y = *ry;
            if (auto rz = ot->get_optional<float>("rz")) offsetTransform.rotation.z = *rz;
            if (auto sx = ot->get_optional<float>("sx")) offsetTransform.scale.x = *sx;
            if (auto sy = ot->get_optional<float>("sy")) offsetTransform.scale.y = *sy;
        }
    } catch (const std::exception& e) {
        return std::string(e.what());
    }
    return std::nullopt;
}

void Node::serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const {
    if (has_flag(flags, SerializeNodeFlags::Basics)) {
        serializer.putKey("uuid");
        serializer.putValue(uuid);
        serializer.putKey("name");
        serializer.putValue(name);
        serializer.putKey("type");
        serializer.putValue(typeId());
    }
    if (has_flag(flags, SerializeNodeFlags::State)) {
        serializer.putKey("enabled");
        serializer.putValue(enabled);
        serializer.putKey("zsort");
        serializer.putValue(zsortRel);
        serializer.putKey("offsetSort");
        serializer.putValue(offsetSort);
        ::nicxlive::core::serde::InochiSerializer tser;
        tser.putKey("x"); tser.putValue(localTransform.translation.x);
        tser.putKey("y"); tser.putValue(localTransform.translation.y);
        tser.putKey("z"); tser.putValue(localTransform.translation.z);
        tser.putKey("rx"); tser.putValue(localTransform.rotation.x);
        tser.putKey("ry"); tser.putValue(localTransform.rotation.y);
        tser.putKey("rz"); tser.putValue(localTransform.rotation.z);
        tser.putKey("sx"); tser.putValue(localTransform.scale.x);
        tser.putKey("sy"); tser.putValue(localTransform.scale.y);
        serializer.root.add_child("transform", tser.root);
        ::nicxlive::core::serde::InochiSerializer ot;
        ot.putKey("x"); ot.putValue(offsetTransform.translation.x);
        ot.putKey("y"); ot.putValue(offsetTransform.translation.y);
        ot.putKey("z"); ot.putValue(offsetTransform.translation.z);
        ot.putKey("rx"); ot.putValue(offsetTransform.rotation.x);
        ot.putKey("ry"); ot.putValue(offsetTransform.rotation.y);
        ot.putKey("rz"); ot.putValue(offsetTransform.rotation.z);
        ot.putKey("sx"); ot.putValue(offsetTransform.scale.x);
        ot.putKey("sy"); ot.putValue(offsetTransform.scale.y);
        serializer.root.add_child("offsetTransform", ot.root);
        serializer.putKey("lockToRoot");
        serializer.putValue(lockToRoot);
        serializer.putKey("pinToMesh");
        serializer.putValue(pinToMesh);
    }
    if (recursive && has_flag(flags, SerializeNodeFlags::Children) && !children.empty()) {
        boost::property_tree::ptree arr;
        for (auto& child : children) {
            if (!child) continue;
            if (child->typeId() == "Tmp") continue;
            ::nicxlive::core::serde::InochiSerializer cs;
            child->serializeSelfImpl(cs, true, flags);
            arr.push_back(std::make_pair("", cs.root));
        }
        serializer.root.add_child("children", arr);
    }
}

void Node::serializeSelf(::nicxlive::core::serde::InochiSerializer& serializer) const {
    serializeSelfImpl(serializer, true, SerializeNodeFlags::All);
}

void Node::serializePartial(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive) const {
    serializeSelfImpl(serializer, recursive, SerializeNodeFlags::All);
}

void Node::serializePartial(::nicxlive::core::serde::InochiSerializer& serializer, SerializeNodeFlags flags, bool recursive) const {
    serializeSelfImpl(serializer, recursive, flags);
}

::nicxlive::core::serde::SerdeException Node::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    try {
        if (auto v = data.get_optional<uint32_t>("uuid")) uuid = *v;
        if (auto n = data.get_optional<std::string>("name")) name = *n;
        if (auto en = data.get_optional<bool>("enabled")) enabled = *en;
        if (auto zs = data.get_optional<float>("zsort")) zsortRel = *zs;
        if (auto os = data.get_optional<float>("offsetSort")) offsetSort = *os;
        if (auto ltr = data.get_optional<bool>("lockToRoot")) lockToRoot = *ltr;
        if (auto ptm = data.get_optional<bool>("pinToMesh")) pinToMesh = *ptm;
        if (auto t = data.get_child_optional("transform")) {
            deserializeTransform(*t);
        }
        if (auto ot = data.get_child_optional("offsetTransform")) {
            if (auto tx = ot->get_optional<float>("x")) offsetTransform.translation.x = *tx;
            if (auto ty = ot->get_optional<float>("y")) offsetTransform.translation.y = *ty;
            if (auto tz = ot->get_optional<float>("z")) offsetTransform.translation.z = *tz;
            if (auto rx = ot->get_optional<float>("rx")) offsetTransform.rotation.x = *rx;
            if (auto ry = ot->get_optional<float>("ry")) offsetTransform.rotation.y = *ry;
            if (auto rz = ot->get_optional<float>("rz")) offsetTransform.rotation.z = *rz;
            if (auto sx = ot->get_optional<float>("sx")) offsetTransform.scale.x = *sx;
            if (auto sy = ot->get_optional<float>("sy")) offsetTransform.scale.y = *sy;
        }
        if (auto childrenTree = data.get_child_optional("children")) {
            for (const auto& childNode : *childrenTree) {
                std::string type = childNode.second.get<std::string>("type", "Node");
                if (!Node::inHasNodeType(type)) continue;
                auto child = Node::inInstantiateNode(type);
                child->deserializeFromFghj(childNode.second);
                addChild(child);
            }
        }
    } catch (const std::exception& e) {
        return std::string(e.what());
    }
    return std::nullopt;
}

} // namespace nicxlive::core::nodes
