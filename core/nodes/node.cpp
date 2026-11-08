#include "../render/profiler.hpp"
#include "node.hpp"
#include "../compat/source_c_string.hpp"
#include "../dbg.hpp"
#include "../compat/default_random.hpp"
#include "../compat/source_allocator.hpp"

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
#include "shapes.hpp"
#include "../debug_log.hpp"
#include "../common/string_key_map.hpp"
#include "node_task_sort.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <limits>

namespace nicxlive::core::nodes {

namespace {
thread_local std::vector<uint32_t> g_takenUUIDs{};
constexpr uint32_t kInvalidUUID = std::numeric_limits<uint32_t>::max();
using NodeFactory = std::shared_ptr<Node::ParentNodeFactory>;
common::StringKeyMap<NodeFactory>& nodeFactories() {
    // SimplePhysics and Shapes use D's per-thread InNode module constructors.
    // The other built-ins are registered explicitly by initRendererCommon.
    static thread_local common::StringKeyMap<NodeFactory> map = [] {
        common::StringKeyMap<NodeFactory> initial;
        initial.setRetained(d_runtime::RetainedString::fromStaticLiteral("SimplePhysics"),
            std::make_shared<Node::ParentNodeFactory>([](const std::shared_ptr<Node>& parent) {
                return SimplePhysicsDriver::create(parent);
            }));
        initial.setRetained(d_runtime::RetainedString::fromStaticLiteral("Shapes"),
            std::make_shared<Node::ParentNodeFactory>([](const std::shared_ptr<Node>& parent) {
                return Shapes::create(parent);
            }));
        return initial;
    }();
    return map;
}

void registerDefaultNodes() {
    Node::inRegisterNodeTypeRetained(d_runtime::RetainedString::fromStaticLiteral("Node"),
        Node::ParentNodeFactory([](const std::shared_ptr<Node>& parent) {
            return Node::create(parent);
        }));
    Node::inRegisterNodeTypeRetained(d_runtime::RetainedString::fromStaticLiteral("Tmp"),
        Node::ParentNodeFactory([](const std::shared_ptr<Node>& parent) {
            return TmpNode::create(parent);
        }));
    Node::inRegisterNodeTypeRetained(d_runtime::RetainedString::fromStaticLiteral("Part"),
        Node::ParentNodeFactory([](const std::shared_ptr<Node>& parent) {
            return Part::create(parent);
        }));
    Node::inRegisterNodeTypeRetained(d_runtime::RetainedString::fromStaticLiteral("Mask"),
        Node::ParentNodeFactory([](const std::shared_ptr<Node>& parent) {
            return Mask::create(parent);
        }));
    Node::inRegisterNodeTypeRetained(d_runtime::RetainedString::fromStaticLiteral("Composite"),
        Node::ParentNodeFactory([](const std::shared_ptr<Node>& parent) {
            return Composite::create(parent);
        }));
    Node::inRegisterNodeTypeRetained(d_runtime::RetainedString::fromStaticLiteral("MeshGroup"),
        Node::ParentNodeFactory([](const std::shared_ptr<Node>& parent) {
            return MeshGroup::create(parent);
        }));
    Node::inRegisterNodeTypeRetained(d_runtime::RetainedString::fromStaticLiteral("DynamicComposite"),
        Node::ParentNodeFactory([](const std::shared_ptr<Node>& parent) {
            return DynamicComposite::create(parent);
        }));
    Node::inRegisterNodeTypeRetained(d_runtime::RetainedString::fromStaticLiteral("GridDeformer"),
        Node::ParentNodeFactory([](const std::shared_ptr<Node>& parent) {
            return GridDeformer::create(parent);
        }));
    Node::inRegisterNodeTypeRetained(d_runtime::RetainedString::fromStaticLiteral("PathDeformer"),
        Node::ParentNodeFactory([](const std::shared_ptr<Node>& parent) {
            return PathDeformer::create(parent);
        }));
    Node::inAliasNodeTypeRetained(d_runtime::RetainedString::fromStaticLiteral("PathDeformer"),
        d_runtime::RetainedString::fromStaticLiteral("BezierDeformer"));
}

} // namespace

// Debug line storage (for drawOrientation / drawBounds parity)
static std::vector<DebugLine> g_debugLines{};

std::vector<DebugLine>& debugDrawBuffer() { return g_debugLines; }
void clearDebugDrawBuffer() { g_debugLines.clear(); }

void Node::constructWithParent(uint32_t id, const std::shared_ptr<Node>& parentNode) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto actualParent = parentNode;
    insertInto(actualParent, OFFSET_END);
    uuid = id;
}

void Node::constructWithParent(const std::shared_ptr<Node>& parentNode) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto actualParent = parentNode;
    const auto id = inCreateUUID();
    constructWithParent(id, actualParent);
}

std::shared_ptr<Node> Node::allocateSourceReceiver() {
    return std::allocate_shared<Node>(d_runtime::SourceAllocator<Node>{});
}

std::shared_ptr<Node> Node::create(const std::shared_ptr<Node>& parentNode) {
    const auto actualParent = parentNode;
    auto node = std::make_shared<Node>();
    node->constructWithParent(actualParent);
    return node;
}

std::shared_ptr<Node> Node::create(uint32_t id, const std::shared_ptr<Node>& parentNode) {
    const auto actualParent = parentNode;
    auto node = std::make_shared<Node>();
    node->constructWithParent(id, actualParent);
    return node;
}

void Node::constructWithPuppet(const std::shared_ptr<::nicxlive::core::Puppet>& owner) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto actualOwner = owner;
    // The source Puppet constructor only assigns its actual Puppet reference.
    puppet = actualOwner;
}

std::shared_ptr<Node> Node::createForPuppet(const std::shared_ptr<::nicxlive::core::Puppet>& owner) {
    const auto actualOwner = owner;
    auto node = std::make_shared<Node>();
    node->constructWithPuppet(actualOwner);
    return node;
}

bool Node::renderEnabled() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (auto p = parent.lock()) {
        return p->renderEnabled() && enabled;
    }
    return enabled;
}

bool Node::renderEnabled() const {
    return const_cast<Node*>(this)->renderEnabled();
}

d_runtime::RetainedString Node::cNameRetained() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto actualName = d_runtime::RetainedString(getName());
    const d_runtime::SourceCString<479> result(actualName);
    return result.retainedString();
}

common::NullableString Node::typeId() const {
    return common::NullableString::fromStaticLiteral("Node");
}

float Node::zSortBase() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (const auto actualParent = parentRef()) return actualParent->zSort();
    return 0.0f;
}

float Node::zSortBase() const {
    return const_cast<Node*>(this)->zSortBase();
}

float& Node::relZSort() {
    return zsortRel;
}

float Node::relZSort() const {
    return const_cast<Node*>(this)->relZSort();
}

float Node::zSortNoOffset() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto base = zSortBase();
    const auto relative = relZSort();
    return base + relative;
}

float Node::zSortNoOffset() const {
    return const_cast<Node*>(this)->zSortNoOffset();
}

float Node::zSort() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto base = zSortBase();
    const auto relative = relZSort();
    return base + relative + offsetSort;
}

float Node::zSort() const {
    return const_cast<Node*>(this)->zSort();
}

void Node::zSort(float value) {
    zsortRel = value;
}

void Node::setAbsZSort(float value) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto base = zSortBase();
    zsortRel = value - base - offsetSort;
}

void Node::setParent(const std::shared_ptr<Node>& p) {
    parent = p;
}

void Node::setParentSource(std::shared_ptr<Node> parent) {
    insertInto(parent, OFFSET_END);
}

std::shared_ptr<Node> Node::parentPtr() const {
    return parent.lock();
}

void Node::addChild(const std::shared_ptr<Node>& child) {
    const auto actualChild = child;
    const auto receiver = shared_from_this();
    d_runtime::ngRequireObject(actualChild)->insertInto(receiver, OFFSET_END);
}

void Node::clearChildren() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto sourceChildren = childrenList();
    for (auto child : sourceChildren) {
        d_runtime::ngRequireObject(child)->parent.reset();
    }
    children.clear();
}

std::size_t Node::getIndexInParent() const {
    const auto receiver = std::const_pointer_cast<Node>(weak_from_this().lock());
    const auto currentParent = d_runtime::ngRequireObject(parent.lock());
    const auto range = currentParent->children;
    for (std::size_t i = 0; i < range.size(); ++i) {
        const auto candidate = range[i];
        if (receiver ? ngResourceObjectEquals(candidate, receiver) : candidate.get() == this) return i;
    }
    return static_cast<std::size_t>(-1);
}

std::size_t Node::getIndexInNode(const std::shared_ptr<Node>& n) const {
    const auto receiver = std::const_pointer_cast<Node>(weak_from_this().lock());
    const auto node = d_runtime::ngRequireObject(n);
    const auto range = node->children;
    for (std::size_t i = 0; i < range.size(); ++i) {
        const auto candidate = range[i];
        if (receiver ? ngResourceObjectEquals(candidate, receiver) : candidate.get() == this) return i;
    }
    return static_cast<std::size_t>(-1);
}

std::ptrdiff_t Node::getIndexInParentSource() {
    [[maybe_unused]] const auto receiverLifetime = shared_from_this();
    const auto index = getIndexInParent();
    return index == std::numeric_limits<std::size_t>::max()
        ? std::ptrdiff_t{-1} : static_cast<std::ptrdiff_t>(index);
}

std::ptrdiff_t Node::getIndexInNodeSource(std::shared_ptr<Node> node) {
    [[maybe_unused]] const auto receiverLifetime = shared_from_this();
    const auto index = getIndexInNode(node);
    return index == std::numeric_limits<std::size_t>::max()
        ? std::ptrdiff_t{-1} : static_cast<std::ptrdiff_t>(index);
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

void Node::inInitBuiltinNodeTypes() {
    registerDefaultNodes();
}

void Node::inInitNodesSource() {
    inRegisterNodeTypeRetained(d_runtime::RetainedString::fromStaticLiteral("Node"),
        ParentNodeFactory([](const std::shared_ptr<Node>& parent) {
            return Node::create(parent);
        }));
    inRegisterNodeTypeRetained(d_runtime::RetainedString::fromStaticLiteral("Tmp"),
        ParentNodeFactory([](const std::shared_ptr<Node>& parent) {
            return TmpNode::create(parent);
        }));
}

bool Node::inHasNodeType(const std::string& id) {
    return nodeFactories().contains(id);
}

void Node::inRegisterNodeType(const std::string& id, const std::function<std::shared_ptr<Node>()>& factory) {
    // Legacy factories attach after construction; parent-aware registrations own
    // constructor-time parent handling and must not be attached a second time.
    nodeFactories().set(id, std::make_shared<ParentNodeFactory>([factory](const std::shared_ptr<Node>& parent) {
        auto node = factory();
        if (parent && node) parent->addChild(node);
        return node;
    }));
}

void Node::inRegisterNodeType(const std::string& id, const ParentNodeFactory& factory) {
    nodeFactories().set(id, std::make_shared<ParentNodeFactory>(factory));
}

void Node::inAliasNodeType(const std::string& existingId, const std::string& aliasId) {
    inAliasNodeTypeRetained(d_runtime::RetainedString(existingId), d_runtime::RetainedString(aliasId));
}

std::shared_ptr<Node> Node::inInstantiateNode(const std::string& id, const std::shared_ptr<Node>& parent) {
    return inInstantiateNodeRetained(d_runtime::RetainedString(id), parent);
}

std::shared_ptr<Node> Node::inInstantiateNodeRetained(d_runtime::RetainedString id,
    const std::shared_ptr<Node>& parent) {
    const auto actualParent = parent;
    const auto entry = nodeFactories().findRetained(id.view());
    if (!entry) throw d_runtime::ngStaticError<d_runtime::RangeError>(
        d_runtime::RetainedString::fromStaticLiteral("source/nijilive/core/nodes/node.d"), 1442);
    const auto factory = *entry;
    return (*d_runtime::ngRequireObject(factory))(actualParent);
}

bool Node::inHasNodeTypeRetained(d_runtime::RetainedString id) {
    return nodeFactories().contains(id.view());
}

void Node::inRegisterNodeTypeRetained(d_runtime::RetainedString id, const ParentNodeFactory& factory) {
    nodeFactories().setRetained(id, std::make_shared<ParentNodeFactory>(factory));
}

void Node::inAliasNodeTypeRetained(d_runtime::RetainedString existingId, d_runtime::RetainedString aliasId) {
    if (nodeFactories().contains(existingId.view())) {
        const auto factory = nodeFactories().get(existingId.view());
        nodeFactories().setRetained(aliasId, factory);
    }
}

std::shared_ptr<::nicxlive::core::Puppet> Node::puppetRef() const {
    if (auto p = parent.lock()) {
        return p->puppetRef();
    }
    return puppet.lock();
}

void Node::preProcess() {
    auto profile = core::render::profileScope("Node.preProcess");
    const auto lifetime = weak_from_this().lock();
    if (preProcessed) return;
    preProcessed = true;
    const auto filters = preProcessFilters;
    for (const auto hook : filters) {
        const auto matrixOwner = overrideTransformMatrix;
        Mat4 matrix = Mat4::identity();
        if (auto p = parent.lock()) {
            matrix = matrixOwner ? matrixOwner->matrix : p->transform().toMat4();
        }
        const Vec2 localPoint{localTransform.translation.x, localTransform.translation.y};
        Vec2Array localTrans;
        localTrans.resize(1);
        localTrans.set(0, localPoint);
        const Vec2 offsetPoint{offsetTransform.translation.x, offsetTransform.translation.y};
        Vec2Array offsetTrans;
        offsetTrans.resize(1);
        offsetTrans.set(0, offsetPoint);
        auto result = hook.func(shared_from_this(), localTrans, offsetTrans, &matrix);
        [[maybe_unused]] const auto returnedMatrixOwner = overrideTransformMatrix;
        const auto& newOffset = std::get<0>(result);
        const auto& newMat = std::get<1>(result);
        bool notify = std::get<2>(result);
        if (!newOffset.empty()) {
            const Vec2 newTranslation{newOffset.xAt(0), newOffset.yAt(0)};
            offsetTransform.translation = Vec3{newTranslation.x, newTranslation.y, offsetTransform.translation.z};
            transformChanged();
        }
        if (newMat != nullptr) {
            overrideTransformMatrix = std::make_shared<MatrixHolder>(*newMat);
        }
        if (notify) {
            notifyChange(shared_from_this());
        }
    }
}

void Node::postProcess(int id) {
    const auto lifetime = weak_from_this().lock();
    if (postProcessed >= id) return;
    postProcessed = id;
    const auto filters = postProcessFilters;
    for (const auto hook : filters) {
        if (hook.stage != id) continue;
        const auto matrixOwner = overrideTransformMatrix;
        Mat4 matrix = Mat4::identity();
        if (auto p = parent.lock()) {
            matrix = matrixOwner ? matrixOwner->matrix : p->transform().toMat4();
        }
        const Vec2 localPoint{localTransform.translation.x, localTransform.translation.y};
        Vec2Array localTrans;
        localTrans.resize(1);
        localTrans.set(0, localPoint);
        const Vec2 offsetPoint{offsetTransform.translation.x, offsetTransform.translation.y};
        Vec2Array offsetTrans;
        offsetTrans.resize(1);
        offsetTrans.set(0, offsetPoint);
        auto result = hook.func(shared_from_this(), localTrans, offsetTrans, &matrix);
        [[maybe_unused]] const auto returnedMatrixOwner = overrideTransformMatrix;
        const auto& newOffset = std::get<0>(result);
        const auto& newMat = std::get<1>(result);
        bool notify = std::get<2>(result);
        if (!newOffset.empty()) {
            const Vec2 newTranslation{newOffset.xAt(0), newOffset.yAt(0)};
            offsetTransform.translation = Vec3{newTranslation.x, newTranslation.y, offsetTransform.translation.z};
            transformChanged();
            overrideTransformMatrix = std::make_shared<MatrixHolder>(transform().toMat4());
        }
        if (newMat != nullptr) {
            overrideTransformMatrix = std::make_shared<MatrixHolder>(*newMat);
        }
        if (notify) {
            notifyChange(shared_from_this());
        }
    }
}

Transform Node::transform() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (recalcTransform) {
        localTransform.update();
        offsetTransform.update();
        Transform combined = localTransform.calcOffset(offsetTransform);
        if (lockToRoot) {
            Transform trans{Vec3{0.0f, 0.0f, 0.0f}};
            if (auto pup = puppetRef()) {
                if (auto root = pup->root) {
                    trans = root->localTransform;
                }
            }
            combined = combined * trans;
        } else if (auto p = parent.lock()) {
            combined = combined * p->transform();
        }
        globalTransform = combined;
        // Prefer explicit override matrix for dynamic matrix consumers.
        if (overrideTransformMatrix) {
            cachedWorld = overrideTransformMatrix->matrix;
        } else {
            // Apply one-time transform only to cached world matrix.
            Mat4 mat = combined.toMat4();
            if (oneTimeTransformPtr) {
                mat = Mat4::multiply(mat, *oneTimeTransformPtr);
            }
            cachedWorld = mat;
        }
        recalcTransform = false;
    }
    return globalTransform;
}

Transform Node::transform() const {
    return const_cast<Node*>(this)->transform();
}

Transform Node::transformLocal() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    localTransform.update();
    return localTransform.calcOffset(offsetTransform);
}

Transform Node::transformLocal() const {
    return const_cast<Node*>(this)->transformLocal();
}

Transform Node::transformNoLock() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    localTransform.update();
    if (auto p = parent.lock()) {
        return localTransform * p->transform();
    }
    return localTransform;
}

Transform Node::transformNoLock() const {
    return const_cast<Node*>(this)->transformNoLock();
}

void Node::setRelativeTo(const std::shared_ptr<Node>& to) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto destination = to;
    setRelativeTo(d_runtime::ngRequireObject(destination)->transform().toMat4());
    const auto currentSort = zSortNoOffset();
    const auto destinationSort = d_runtime::ngRequireObject(destination)->zSortNoOffset();
    zSort(currentSort - destinationSort);
}

void Node::setRelativeTo(const Mat4& to) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto actualMatrix = to;
    const auto currentMatrix = transformNoLock().toMat4();
    auto rel = getRelativePosition(actualMatrix, currentMatrix);
    localTransform.translation = rel;
    localTransform.update();
}

void Node::setRelativeTo(const Mat4& to, float zsortBase) {
    setRelativeTo(to);
    setAbsZSort(zsortBase);
}

Vec3 Node::getRelativePosition(const Mat4& m1, const Mat4& m2) {
    const auto first = m1;
    const auto second = m2;
    Mat4 inv = Mat4::inverse(first);
    Mat4 cm = Mat4::multiply(inv, second);
    return Vec3{cm[0][3], cm[1][3], cm[2][3]};
}

Vec3 Node::getRelativePositionInv(const Mat4& m1, const Mat4& m2) {
    const auto first = m1;
    const auto second = m2;
    Mat4 cm = Mat4::multiply(second, Mat4::inverse(first));
    return Vec3{cm[0][3], cm[1][3], cm[2][3]};
}

d_runtime::RetainedString Node::getNodePathRetained() {
    const auto receiver = shared_from_this();
    if (!nodePathCache.empty()) return d_runtime::RetainedString(nodePathCache);
    common::SharedArray<d_runtime::RetainedString> segments;
    auto current = receiver;
    while (current) {
        const auto name = d_runtime::RetainedString(current->getName());
        const auto previous = segments;
        common::SharedArray<d_runtime::RetainedString> prepended(
            previous.size() + 1, d_runtime::RetainedString{});
        prepended[0] = name;
        for (std::size_t i = 0; i < previous.size(); ++i) prepended[i + 1] = previous[i];
        segments = prepended;
        current = current->parentRef();
    }
    std::string joined;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        if (i) joined += '/';
        const auto segment = segments[i];
        if (!segment.empty()) joined.append(segment.data(), segment.size());
    }
    nodePathCache = common::NullableString(std::string("/") + joined);
    return d_runtime::RetainedString(nodePathCache);
}

std::string Node::getNodePath() {
    const auto path = getNodePathRetained();
    return std::string(path.view());
}

int Node::depth() const {
    std::uint32_t value = 1;
    auto current = parent.lock();
    while (current) {
        ++value;
        current = current->parent.lock();
    }
    return std::bit_cast<std::int32_t>(value);
}

void Node::insertInto(const std::shared_ptr<Node>& node, std::size_t offset) {
    const auto destination = node;
    const auto receiver = shared_from_this();
    nodePathCache = nullptr;
    if (auto p = parent.lock()) {
        const auto range = p->children;
        std::ptrdiff_t found = -1;
        for (std::size_t i = 0; i < range.size(); ++i) {
            const auto candidate = range[i];
            if (ngResourceObjectEquals(candidate, receiver)) {
                found = static_cast<std::ptrdiff_t>(i);
                break;
            }
        }
        if (found >= 0) {
            const auto currentParent = d_runtime::ngRequireObject(parent.lock());
            const auto current = currentParent->children;
            const auto index = static_cast<std::size_t>(found);
            const auto length = current.size();
#ifndef NDEBUG
            if (index >= length) throw d_runtime::ngStaticError<d_runtime::AssertError>(
                d_runtime::RetainedString::fromStaticLiteral("range is smaller than amount of items to pop"),
                d_runtime::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2137);
#else
            if (index > length) throw d_runtime::ngStaticError<d_runtime::ArraySliceError>(index, length, length,
                d_runtime::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
            if (index == length) throw d_runtime::ngStaticError<d_runtime::ArraySliceError>(1, 0, 0,
                d_runtime::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
#endif
            currentParent->children = current.removed(index);
        }
    }

    if (!destination) {
        parent.reset();
        return;
    }

    parent = destination;
    auto& dst = destination->children;
    if (offset == OFFSET_START) {
        const auto old = dst;
        ChildArray replacement(old.size() + 1, nullptr);
        replacement[0] = receiver;
        for (std::size_t i = 0; i < old.size(); ++i) replacement[i + 1] = old[i];
        dst = replacement;
    } else if (offset == OFFSET_END || offset >= dst.size()) {
        dst.push_back(receiver);
    } else {
        const auto old = dst;
        ChildArray replacement(old.size() + 1, nullptr);
        for (std::size_t i = 0; i < offset; ++i) replacement[i] = old[i];
        replacement[offset] = receiver;
        for (std::size_t i = offset; i < old.size(); ++i) replacement[i + 1] = old[i];
        dst = replacement;
    }
    if (puppetRef()) {
        d_runtime::ngRequireObject(puppetRef())->rescanNodes();
    }
}

void Node::setLockToRoot(bool value) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (value && !lockToRoot) {
        localTransform.translation = transformNoLock().translation;
    } else if (!value && lockToRoot) {
        const auto parentTransform = d_runtime::ngRequireObject(parent.lock())->transformNoLock();
        localTransform.translation = {
            localTransform.translation.x - parentTransform.translation.x,
            localTransform.translation.y - parentTransform.translation.y,
            localTransform.translation.z - parentTransform.translation.z};
    }
    lockToRoot = value;
}

void Node::setPinToMesh(bool value) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
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

bool Node::hasParamSource(d_runtime::RetainedString sourceKey) {
    const auto self = shared_from_this();
    const auto key = sourceKey.view();
    return key == "zSort" || key == "transform.t.x" || key == "transform.t.y" || key == "transform.t.z"
        || key == "transform.r.x" || key == "transform.r.y" || key == "transform.r.z"
        || key == "transform.s.x" || key == "transform.s.y";
}

float Node::getDefaultValueSource(d_runtime::RetainedString sourceKey) {
    const auto self = shared_from_this();
    const auto key = sourceKey.view();
    if (key == "zSort" || key == "transform.t.x" || key == "transform.t.y" || key == "transform.t.z" ||
        key == "transform.r.x" || key == "transform.r.y" || key == "transform.r.z") return 0.0f;
    if (key == "transform.s.x" || key == "transform.s.y") return 1.0f;
    return std::numeric_limits<float>::quiet_NaN();
}

bool Node::setValueSource(d_runtime::RetainedString sourceKey, float value) {
    const auto self = shared_from_this();
    const auto key = sourceKey.view();
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

float Node::scaleValueSource(d_runtime::RetainedString sourceKey, float value, int axis, float scale) {
    const auto self = shared_from_this();
    const auto key = sourceKey.view();
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

float Node::getValueSource(d_runtime::RetainedString sourceKey) {
    const auto self = shared_from_this();
    const auto key = sourceKey.view();
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
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    bool changedFlag = enabled != value;
    enabled = value;
    if (changedFlag) {
        notifyChange(shared_from_this(), NotifyReason::AttributeChanged);
    }
}

void Node::draw() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (!renderEnabled()) return;
    const auto sourceChildren = childrenList();
    for (auto child : sourceChildren) {
        d_runtime::ngRequireObject(child)->draw();
    }
}

void Node::reconstruct() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto sourceChildren = children.duplicate();
    for (auto child : sourceChildren) {
        d_runtime::ngRequireObject(child)->reconstruct();
    }
}

void Node::finalize() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto sourceChildren = childrenList();
    for (auto child : sourceChildren) {
        d_runtime::ngRequireObject(child)->finalize();
    }
}

void Node::transformChanged() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    recalcTransform = true;
    const auto sourceChildren = childrenList();
    for (auto child : sourceChildren) {
        d_runtime::ngRequireObject(child)->transformChanged();
    }
}

void Node::resetMask() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (auto p = parent.lock()) {
        p->resetMask();
    }
}

NodeNotifyListener NodeNotifyListener::forNode(const std::shared_ptr<Node>& listener) {
    return fromMethod<&Node::notifyChange>(listener);
}

void Node::notifyChange(const std::shared_ptr<Node>& target, NotifyReason reason) {
    const auto receiver = weak_from_this().lock();
    const auto actualTarget = target;
    if (actualTarget.get() == this || (receiver && ngResourceObjectEquals(actualTarget, receiver))) changed = true;
    if (auto pup = puppetRef()) pup->recordNodeChange(reason);
    if (changeDeferred) {
        changePool.push_back({actualTarget, reason});
    } else {
        if (auto p = parent.lock()) p->notifyChange(actualTarget, reason);
        const auto listeners = notifyListeners;
        for (auto listener : listeners) listener(actualTarget, reason);
    }
}

void Node::addNotifyListener(NotifyListener listener) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (!listener) return;
    const auto listeners = notifyListeners;
    for (const auto existing : listeners) {
        if (existing == listener) return;
    }
    notifyListeners.push_back(listener);
}

void Node::removeNotifyListener(NotifyListener listener) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    auto listeners = notifyListeners;
    for (std::size_t index = 0; index < listeners.size(); ++index) {
        if (listeners[index] == listener) {
            listeners.erase(listeners.begin() + index);
            notifyListeners = listeners;
            return;
        }
    }
}

void Node::addNotifyListener(const std::shared_ptr<Node>& listener) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    addNotifyListener(NotifyListener::forNode(listener));
}

void Node::removeNotifyListener(const std::shared_ptr<Node>& listener) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    removeNotifyListener(NotifyListener::forNode(listener));
}

void Node::runBeginTask(core::RenderContext&) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    preProcessed = false;
    postProcessed = -1;
    changed = false;
    changeDeferred = true;
    changePool.resize(0);
    offsetSort = 0.0f;
    offsetTransform.clear();
    overrideTransformMatrix.reset();
}

void Node::runPreProcessTask(core::RenderContext&) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    preProcess();
}

void Node::runDynamicTask(core::RenderContext&) {
    // The Source base dynamic task has an empty body.
}

void Node::runPostTaskImpl(std::size_t priority, core::RenderContext&) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    postProcess(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(priority)));
}

void Node::runPostTask0() { core::RenderContext dummy{}; runPostTaskImpl(0, dummy); }
void Node::runPostTask1() { core::RenderContext dummy{}; runPostTaskImpl(1, dummy); }
void Node::runPostTask2() { core::RenderContext dummy{}; runPostTaskImpl(2, dummy); }

void Node::runFinalTask(core::RenderContext&) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    postProcess(-1);
    flushNotifyChange();
}

void Node::runRenderTask(core::RenderContext&) {}
void Node::runRenderBeginTask(core::RenderContext&) {}
void Node::runRenderEndTask(core::RenderContext&) {}

void Node::runPostTask0(core::RenderContext& ctx) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    runPostTaskImpl(0, ctx);
}
void Node::runPostTask1(core::RenderContext& ctx) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    runPostTaskImpl(1, ctx);
}
void Node::runPostTask2(core::RenderContext& ctx) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    runPostTaskImpl(2, ctx);
}

void Node::registerRenderTasks(std::shared_ptr<core::TaskScheduler> scheduler) {
    if (!scheduler) return;
    const auto self = shared_from_this();
    scheduler->addTask(core::TaskOrder::Init, core::TaskKind::Init,
        [self](core::RenderContext& ctx) { self->runBeginTask(ctx); });
    auto hasFlag = [&](NodeTaskFlag flag) { return has_flag(taskFlags, flag); };
    const bool needPreProcess = !preProcessFilters.empty() || hasFlag(NodeTaskFlag::PreProcess);
    const bool needDynamic = hasFlag(NodeTaskFlag::Dynamic);
    bool needPost[3]{};
    const auto filters = postProcessFilters;
    for (const auto filter : filters) {
        const auto stage = filter.stage;
        if (stage >= 0 && stage < 3) needPost[stage] = true;
    }
    if (hasFlag(NodeTaskFlag::Post0)) needPost[0] = true;
    if (hasFlag(NodeTaskFlag::Post1)) needPost[1] = true;
    if (hasFlag(NodeTaskFlag::Post2)) needPost[2] = true;
    const bool needRenderBegin = hasFlag(NodeTaskFlag::RenderBegin);
    const bool needRender = hasFlag(NodeTaskFlag::Render);
    const bool needRenderEnd = hasFlag(NodeTaskFlag::RenderEnd);
    if (needPreProcess) scheduler->addTask(core::TaskOrder::PreProcess, core::TaskKind::PreProcess,
        [self](core::RenderContext& ctx) { self->runPreProcessTask(ctx); });
    if (needDynamic) scheduler->addTask(core::TaskOrder::Dynamic, core::TaskKind::Dynamic,
        [self](core::RenderContext& ctx) { self->runDynamicTask(ctx); });
    if (needPost[0]) scheduler->addTask(core::TaskOrder::Post0, core::TaskKind::PostProcess,
        [self](core::RenderContext& ctx) { self->runPostTask0(ctx); });
    if (needPost[1]) scheduler->addTask(core::TaskOrder::Post1, core::TaskKind::PostProcess,
        [self](core::RenderContext& ctx) { self->runPostTask1(ctx); });
    if (needPost[2]) scheduler->addTask(core::TaskOrder::Post2, core::TaskKind::PostProcess,
        [self](core::RenderContext& ctx) { self->runPostTask2(ctx); });
    if (needRenderBegin) scheduler->addTask(core::TaskOrder::RenderBegin, core::TaskKind::Render,
        [self](core::RenderContext& ctx) { self->runRenderBeginTask(ctx); });
    if (needRender) scheduler->addTask(core::TaskOrder::Render, core::TaskKind::Render,
        [self](core::RenderContext& ctx) { self->runRenderTask(ctx); });
    scheduler->addTask(core::TaskOrder::Final, core::TaskKind::Finalize,
        [self](core::RenderContext& ctx) { self->runFinalTask(ctx); });
    auto orderedChildren = children.duplicate();
    if (orderedChildren.size() > 1) node_task_sort::sort(orderedChildren);
    for (auto child : orderedChildren)
        core::d_runtime::ngRequireObject(child)->registerRenderTasks(scheduler);
    if (needRenderEnd) scheduler->addTask(core::TaskOrder::RenderEnd, core::TaskKind::Render,
        [self](core::RenderContext& ctx) { self->runRenderEndTask(ctx); });
}

void Node::registerRenderTasks(core::TaskScheduler& scheduler) {
    const auto self = shared_from_this();
    scheduler.addTask(core::TaskOrder::Init, core::TaskKind::Init, [self](core::RenderContext& ctx) { self->runBeginTask(ctx); });

    auto hasFlag = [&](NodeTaskFlag flag) { return has_flag(taskFlags, flag); };
    bool needPreProcess = (!preProcessFilters.empty()) || hasFlag(NodeTaskFlag::PreProcess);
    bool needDynamic = hasFlag(NodeTaskFlag::Dynamic);
    bool needPost[3]{false, false, false};
    const auto postFilters = postProcessFilters;
    for (const auto hook : postFilters) {
        if (hook.stage >= 0 && hook.stage < 3) {
            needPost[hook.stage] = true;
        }
    }
    // The real Deformable filter bank is the native carrier for Source mesh filters.
    // Inspect its current records rather than setting a permanent synthetic task flag.
    if (const auto deformable = dynamic_cast<const Deformable*>(this)) {
        needPreProcess = needPreProcess || !deformable->deformPreProcessFilters.empty();
        const auto filters = deformable->deformPostProcessFilters;
        for (const auto filter : filters) {
            if (filter.stage >= 0 && filter.stage < 3) needPost[filter.stage] = true;
        }
    }
    if (hasFlag(NodeTaskFlag::Post0)) needPost[0] = true;
    if (hasFlag(NodeTaskFlag::Post1)) needPost[1] = true;
    if (hasFlag(NodeTaskFlag::Post2)) needPost[2] = true;
    bool needRenderBegin = hasFlag(NodeTaskFlag::RenderBegin);
    bool needRender = hasFlag(NodeTaskFlag::Render);
    bool needRenderEnd = hasFlag(NodeTaskFlag::RenderEnd);

    if (needPreProcess) {
        scheduler.addTask(core::TaskOrder::PreProcess, core::TaskKind::PreProcess, [self](core::RenderContext& ctx) { self->runPreProcessTask(ctx); });
    }
    if (needDynamic) {
        scheduler.addTask(core::TaskOrder::Dynamic, core::TaskKind::Dynamic, [self](core::RenderContext& ctx) { self->runDynamicTask(ctx); });
    }
    if (needPost[0]) scheduler.addTask(core::TaskOrder::Post0, core::TaskKind::PostProcess, [self](core::RenderContext& ctx) { self->runPostTaskImpl(0, ctx); });
    if (needPost[1]) scheduler.addTask(core::TaskOrder::Post1, core::TaskKind::PostProcess, [self](core::RenderContext& ctx) { self->runPostTaskImpl(1, ctx); });
    if (needPost[2]) scheduler.addTask(core::TaskOrder::Post2, core::TaskKind::PostProcess, [self](core::RenderContext& ctx) { self->runPostTaskImpl(2, ctx); });
    if (needRenderBegin) scheduler.addTask(core::TaskOrder::RenderBegin, core::TaskKind::Render, [self](core::RenderContext& ctx) { self->runRenderBeginTask(ctx); });
    if (needRender) scheduler.addTask(core::TaskOrder::Render, core::TaskKind::Render, [self](core::RenderContext& ctx) { self->runRenderTask(ctx); });
    scheduler.addTask(core::TaskOrder::Final, core::TaskKind::Finalize, [self](core::RenderContext& ctx) { self->runFinalTask(ctx); });

    auto orderedChildren = children.duplicate();
    if (orderedChildren.size() > 1) {
        std::sort(orderedChildren.begin(), orderedChildren.end(), [](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
            if (!a && !b) return false;
            if (!a) return false;
            if (!b) return true;
            return a->zSort() > b->zSort();
        });
    }
    for (auto& child : orderedChildren) {
        if (child) child->registerRenderTasks(scheduler);
    }
    if (needRenderEnd) scheduler.addTask(core::TaskOrder::RenderEnd, core::TaskKind::Render, [self](core::RenderContext& ctx) { self->runRenderEndTask(ctx); });
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

namespace {
Vec4 nodeSourceMatrixVector(const Mat4& matrix, Vec4 value) {
    const float input[4]{value.x, value.y, value.z, value.w};
    float output[4]{};
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            output[row] += matrix.a.a[row][column] * input[column];
        }
    }
    return {output[0], output[1], output[2], output[3]};
}
}

std::array<float, 4> Node::getInitialBoundsSize() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto tr = transform();
    return {tr.translation.x, tr.translation.y, tr.translation.x, tr.translation.y};
}

std::array<float, 4> Node::getCombinedBounds(bool reupdate, bool countPuppet) {
    const auto receiver = shared_from_this();
    auto combined = getInitialBoundsSize();
    bool hasCombined = false;
    if (const auto drawable = std::dynamic_pointer_cast<Drawable>(receiver)) {
        if (reupdate) drawable->updateBounds();
        if (!drawable->getVertices().empty() || children.empty()) {
            // Mapped Source Drawable.bounds is always a four-float value.
            const auto nan = std::numeric_limits<float>::quiet_NaN();
            combined = drawable->bounds.value_or(std::array<float, 4>{nan, nan, nan, nan});
            hasCombined = true;
        }
    } else {
        hasCombined = children.empty();
    }
    const auto sourceChildren = childrenList();
    for (auto child : sourceChildren) {
        const auto bounds = d_runtime::ngRequireObject(child)->getCombinedBounds(reupdate);
        if (!hasCombined) {
            combined = bounds;
            hasCombined = true;
        } else {
            if (bounds[0] < combined[0]) combined[0] = bounds[0];
            if (bounds[1] < combined[1]) combined[1] = bounds[1];
            if (bounds[2] > combined[2]) combined[2] = bounds[2];
            if (bounds[3] > combined[3]) combined[3] = bounds[3];
        }
    }
    if (countPuppet) {
        const auto firstPuppet = d_runtime::ngRequireObject(puppetRef());
        const auto firstMatrix = firstPuppet->transform.matrix();
        const auto topLeft = nodeSourceMatrixVector(firstMatrix, {combined[0], combined[1], 0, 1});
        const auto secondPuppet = d_runtime::ngRequireObject(puppetRef());
        const auto secondMatrix = secondPuppet->transform.matrix();
        const auto bottomRight = nodeSourceMatrixVector(secondMatrix, {combined[2], combined[3], 0, 1});
        return {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
    }
    return combined;
}

Rect Node::getCombinedBoundsRect(bool reupdate, bool countPuppet) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto bounds = getCombinedBounds(reupdate, countPuppet);
    return Rect{bounds[0], bounds[1], bounds[2] - bounds[0], bounds[3] - bounds[1]};
}

void Node::drawOrientation() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto matrix = transform().matrix();
    core::dbg::inDbgLineWidth(4);
    {
        Vec3Array points{{0, 0, 0}, {32, 0, 0}};
        const std::vector<std::uint16_t> indices{0, 1};
        core::dbg::inDbgSetBuffer(static_cast<const Vec3Array&>(points), indices);
    }
    core::dbg::inDbgDrawLines({1, 0, 0, 0.7f}, matrix);
    {
        Vec3Array points{{0, 0, 0}, {0, -32, 0}};
        const std::vector<std::uint16_t> indices{0, 1};
        core::dbg::inDbgSetBuffer(static_cast<const Vec3Array&>(points), indices);
    }
    core::dbg::inDbgDrawLines({0, 1, 0, 0.7f}, matrix);
    {
        Vec3Array points{{0, 0, 0}, {0, 0, -32}};
        const std::vector<std::uint16_t> indices{0, 1};
        core::dbg::inDbgSetBuffer(static_cast<const Vec3Array&>(points), indices);
    }
    core::dbg::inDbgDrawLines({0, 0, 1, 0.7f}, matrix);
    core::dbg::inDbgLineWidth(1);
}

void Node::drawBounds() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto bounds = getCombinedBounds();
    const float width = bounds[2] - bounds[0];
    const float height = bounds[3] - bounds[1];
    Vec3Array points{
        {bounds[0], bounds[1], 0}, {bounds[0] + width, bounds[1], 0},
        {bounds[0] + width, bounds[1], 0}, {bounds[0] + width, bounds[1] + height, 0},
        {bounds[0] + width, bounds[1] + height, 0}, {bounds[0], bounds[1] + height, 0},
        {bounds[0], bounds[1] + height, 0}, {bounds[0], bounds[1], 0}};
    core::dbg::inDbgSetBuffer(static_cast<const Vec3Array&>(points));
    core::dbg::inDbgLineWidth(3);
    const auto oneTime = oneTimeTransformPtr;
    if (oneTime) core::dbg::inDbgDrawLines({0.5f, 0.5f, 0.5f, 1}, *oneTime);
    else core::dbg::inDbgDrawLines({0.5f, 0.5f, 0.5f, 1});
    core::dbg::inDbgLineWidth(1);
}

void Node::setOneTimeTransform(const std::shared_ptr<Mat4>& transform) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto actualTransform = transform;
    oneTimeTransformPtr = actualTransform;
    const auto sourceChildren = childrenList();
    for (auto child : sourceChildren) {
        d_runtime::ngRequireObject(child)->setOneTimeTransform(actualTransform);
    }
}

std::shared_ptr<Mat4> Node::getOneTimeTransform() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    return oneTimeTransformPtr;
}

std::shared_ptr<Mat4> Node::getOneTimeTransform() const {
    return const_cast<Node*>(this)->getOneTimeTransform();
}

void Node::centralize() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto initialChildren = childrenList();
    for (auto child : initialChildren) d_runtime::ngRequireObject(child)->centralize();

    std::array<float, 4> bounds;
    Vec4Array childTranslations;
    if (!children.empty()) {
        const auto firstChild = children[0];
        bounds = d_runtime::ngRequireObject(firstChild)->getCombinedBounds();
        const auto sourceChildren = childrenList();
        for (auto child : sourceChildren) {
            const auto current = d_runtime::ngRequireObject(child)->getCombinedBounds();
            bounds[0] = std::min(bounds[0], current[0]);
            bounds[1] = std::min(bounds[1], current[1]);
            bounds[2] = std::max(bounds[2], current[2]);
            bounds[3] = std::max(bounds[3], current[3]);
            const auto matrix = d_runtime::ngRequireObject(child)->transform().toMat4();
            childTranslations.push_back(nodeSourceMatrixVector(matrix, {0, 0, 0, 1}));
        }
    } else {
        const auto tr = transform();
        bounds = {tr.translation.x, tr.translation.y, tr.translation.x, tr.translation.y};
    }
    Vec2 center{(bounds[0] + bounds[2]) / 2, (bounds[1] + bounds[3]) / 2};
    if (const auto actualParent = parentRef()) {
        const auto inverse = Mat4::inverse(actualParent->transform().toMat4());
        const auto transformed = nodeSourceMatrixVector(inverse, {center.x, center.y, 0, 1});
        center = {transformed.x, transformed.y};
    }
    [[maybe_unused]] const Vec2 difference{
        center.x - localTransform.translation.x, center.y - localTransform.translation.y};
    localTransform.translation.x = center.x;
    localTransform.translation.y = center.y;
    transformChanged();

    const auto finalChildren = childrenList();
    for (std::size_t i = 0; i < finalChildren.size(); ++i) {
        const auto child = finalChildren[i];
        const auto inverse = Mat4::inverse(transform().toMat4());
        if (i >= childTranslations.size()) {
            throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
                i, childTranslations.size(), d_runtime::RetainedString::fromStaticLiteral(
                    "source/nijilive/core/nodes/node.d"), 1353);
        }
        const auto translated = nodeSourceMatrixVector(inverse, childTranslations.at(i));
        d_runtime::ngRequireObject(child)->localTransform.translation = {translated.x, translated.y, translated.z};
        d_runtime::ngRequireObject(child)->transformChanged();
    }
}

void Node::reparent(const std::shared_ptr<Node>& newParent, std::size_t offset, bool ignoreTransform) {
    const auto destination = newParent;
    const auto receiver = shared_from_this();
    releaseSelf();
    auto current = parent.lock();
    while (current) {
        if (!current->releaseChild(receiver)) break;
        current = current->parent.lock();
    }
    if (destination && !ignoreTransform) {
        setRelativeTo(destination);
    }
    insertInto(destination, offset);
    if (destination) {
        auto p = destination;
        while (p) {
            if (!p->setupChild(receiver)) break;
            p = p->parent.lock();
        }
    }
    setupSelf();
}

void Node::copyFrom(const Node& src, bool clone, bool deepCopy) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    [[maybe_unused]] const auto sourceLifetime = src.weak_from_this().lock();
    auto& source = const_cast<Node&>(src);
    setName(src.getName() + (clone ? "" : "'"));
    enabled = src.enabled;
    zsortRel = src.zsortRel;
    lockToRoot = src.lockToRoot;
    nodePathCache = src.nodePathCache;
    changed = src.changed;
    if (clone) {
        uuid = src.uuid;
        puppet = src.puppet;
        finalize();
        if (deepCopy) {
            children.resize(0);
            while (!source.children.empty()) {
                const auto first = source.children;
                const auto trans = d_runtime::ngRequireObject(first[0])->localTransform;
                const auto current = source.children;
                const auto childToTransfer = d_runtime::ngRequireObject(current[0]);
                const auto receiver = shared_from_this();
                const auto offset = source.children.size() - 1;
                childToTransfer->reparent(receiver, offset, true);
                const auto tail = source.children;
                const auto index = tail.size() - 1;
                if (index >= tail.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
                    index, tail.size(), d_runtime::RetainedString::fromStaticLiteral(
                        "source/nijilive/core/nodes/node.d"), 1374);
                d_runtime::ngRequireObject(tail[index])->localTransform = trans;
            }
        }
    } else {
        uuid = inCreateUUID();
        if (deepCopy) {
            children.resize(0);
            localTransform = source.transform();
            const auto range = source.children;
            for (std::size_t i = 0; i < range.size(); ++i) {
                const auto srcChild = range[i];
                const auto trans = d_runtime::ngRequireObject(srcChild)->localTransform;
                const auto sourceType = d_runtime::RetainedString(d_runtime::ngRequireObject(srcChild)->typeId());
                auto child = Node::inInstantiateNodeRetained(sourceType, nullptr);
                d_runtime::ngRequireObject(child)->copyFromSource(srcChild, clone, deepCopy);
                d_runtime::ngRequireObject(child)->reparent(shared_from_this(), i, true);
                d_runtime::ngRequireObject(child)->localTransform = trans;
            }
        }
    }
}

void Node::copyFromSource(std::shared_ptr<Node> src, bool clone, bool deepCopy) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    // Retain the actual nullable source through the complete derived dispatch.
    copyFrom(*d_runtime::ngRequireObject(src), clone, deepCopy);
}

std::shared_ptr<Node> Node::dup() {
    const auto receiver = shared_from_this();
    const auto sourceType = d_runtime::RetainedString(typeId());
    auto node = Node::inInstantiateNodeRetained(sourceType, nullptr);
    d_runtime::ngRequireObject(node)->copyFromSource(receiver);
    return node;
}

bool Node::setupChild(const std::shared_ptr<Node>&) {
    // Base Node does not recursively handle child setup.
    return false;
}

bool Node::releaseChild(const std::shared_ptr<Node>&) {
    // Base Node does not recursively handle child release.
    return false;
}

void Node::setupSelf() {
    // Base Node has no self-setup work.
}

void Node::releaseSelf() {
    // Base Node has no self-release work.
}

Mat4 Node::getDynamicMatrix() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto holder = overrideTransformMatrix;
    if (holder) return holder->matrix;
    return transform().matrix();
}

Mat4 Node::getDynamicMatrix() const {
    return const_cast<Node*>(this)->getDynamicMatrix();
}

void Node::build(bool force) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto sourceChildren = childrenList();
    for (auto child : sourceChildren) d_runtime::ngRequireObject(child)->build(force);
}

bool Node::coverOthers() const {
    return false;
}

bool Node::mustPropagate() const {
    return false;
}

uint32_t Node::inCreateUUID() {
    uint32_t id = d_runtime::ngRandomUintExcludingMax();
    while (std::find(g_takenUUIDs.begin(), g_takenUUIDs.end(), id) != g_takenUUIDs.end()) {
        id = d_runtime::ngRandomUintExcludingMax();
    }
    return id;
}

void Node::inUnloadUUID(uint32_t id) {
    auto it = std::find(g_takenUUIDs.begin(), g_takenUUIDs.end(), id);
    if (it != g_takenUUIDs.end()) {
        // Source ignores remove's shorter returned descriptor; keep the length.
        for (auto next = it + 1; next != g_takenUUIDs.end(); ++next) *it++ = *next;
    }
}

void Node::inClearUUIDs() {
    g_takenUUIDs.clear();
}

void Node::flushNotifyChange() {
    const auto receiver = weak_from_this().lock();
    changeDeferred = false;
    const auto records = changePool;
    for (auto record : records) notifyChange(record.first, record.second);
    changePool.resize(0);
}

bool Node::canReparent(const std::shared_ptr<Node>& to) const {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    auto tmp = to;
    while (tmp) {
        const auto targetUuid = tmp->getUUID();
        const auto receiverUuid = getUUID();
        if (targetUuid == receiverUuid) return false;
        tmp = tmp->parentPtr();
    }
    return true;
}

::nicxlive::core::serde::SerdeException Node::deserializeTransform(const ::nicxlive::core::serde::Fghj& data) {
    return localTransform.deserializeFromFghj(data);
}

void Node::serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const {
    if (has_flag(flags, SerializeNodeFlags::Basics)) {
        serializer.putKey("uuid");
        serializer.putValue(uuid);
        serializer.putKey("name");
        serializer.putValue(getName());
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
        localTransform.serialize(tser);
        serializer.root.add_child("transform", tser.root);
        serializer.putKey("lockToRoot");
        serializer.putValue(lockToRoot);
        serializer.putKey("pinToMesh");
        serializer.putValue(pinToMesh);
    }
    if (recursive && has_flag(flags, SerializeNodeFlags::Children) && !children.empty()) {
        boost::property_tree::ptree arr;
        for (auto child : childrenList()) {
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
        localTransform.clear();
        offsetTransform.clear();
        if (auto t = data.get_child_optional("transform")) {
            deserializeTransform(*t);
        }
        if (auto childrenTree = data.get_child_optional("children")) {
            for (const auto& childNode : *childrenTree) {
                std::string type = childNode.second.get<std::string>("type", "Node");
                if (!Node::inHasNodeType(type)) {
                    static std::size_t sUnknownTypeLogCount = 0;
                    if (sUnknownTypeLogCount < 128) {
                        NJCX_DBG_LOG("[nicxlive] node.deserialize unknown-type parent=%u parentType=%s name=%s childType=%s\n",
                                     uuid, typeId().c_str(), name.c_str(), type.c_str());
                        ++sUnknownTypeLogCount;
                    }
                    continue;
                }
                auto child = Node::inInstantiateNode(type, shared_from_this());
                if (!child) continue;
                if (auto childErr = child->deserializeFromFghj(childNode.second)) {
                    return childErr;
                }
            }
        }
    } catch (const std::exception& e) {
        return std::string(e.what());
    }
    return std::nullopt;
}

void Node::serializeSelfImpl(::nicxlive::core::serde::TypedJsonWriter& serializer,
    bool recursive, SerializeNodeFlags flags) const {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    // Source accepts recursive but does not consult it, including for descendants.
    (void)recursive;
    if (has_flag(flags, SerializeNodeFlags::Basics)) {
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("uuid"));
        serializer.putValue(getUUID());
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("name"));
        const auto actualName = getName();
        serializer.putValue(actualName);
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("type"));
        const auto actualType = typeId();
        serializer.putValue(actualType);
    }
    if (has_flag(flags, SerializeNodeFlags::State)) {
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("enabled"));
        serializer.putValue(enabled);
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("zsort"));
        serializer.putValue(zsortRel);
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("transform"));
        serializer.serializeValue(localTransform);
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("lockToRoot"));
        serializer.serializeValue(lockToRoot);
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("pinToMesh"));
        serializer.serializeValue(pinToMesh);
    }
    if (has_flag(flags, SerializeNodeFlags::Children) && !children.empty()) {
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("children"));
        auto childArray = serializer.listBegin();
        const auto sourceChildren = childrenList();
        for (auto child : sourceChildren) {
            if (std::dynamic_pointer_cast<TmpNode>(child)) continue;
            serializer.elemBegin();
            auto childState = serializer.structBegin();
            d_runtime::ngRequireObject(child)->serializeSelfImpl(serializer, true, flags);
            serializer.structEnd(childState);
        }
        serializer.listEnd(childArray);
    }
}

void Node::serializeSelf(::nicxlive::core::serde::TypedJsonWriter& serializer) const {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    serializeSelfImpl(serializer, true, SerializeNodeFlags::All);
}

void Node::serialize(::nicxlive::core::serde::TypedJsonWriter& serializer) const {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    auto state = serializer.structBegin();
    serializeSelf(serializer);
    serializer.structEnd(state);
}

void Node::serializePartial(::nicxlive::core::serde::TypedJsonWriter& serializer, bool recursive) const {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    serializeSelfImpl(serializer, recursive, SerializeNodeFlags::All);
}

void Node::serializePartial(::nicxlive::core::serde::TypedJsonWriter& serializer,
    SerializeNodeFlags flags, bool recursive) const {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    serializeSelfImpl(serializer, recursive, flags);
}

::nicxlive::core::serde::ReadError Node::deserializeFromFghj(::nicxlive::core::serde::TypedJsonView data) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    namespace serde = ::nicxlive::core::serde;
    if (auto error = serde::deserializeValue(data.findFirst("uuid"), uuid)) return error;
    const auto nameData = data.findFirst("name");
    if (!nameData.isMissing()) {
        d_runtime::RetainedString decoded;
        if (auto error = serde::deserializeValue(nameData, decoded)) return error;
        name = decoded.toNullableString();
        const d_runtime::SourceCString<479> terminated(name.view());
        name = terminated.retainedString().toNullableString();
    }
    if (auto error = serde::deserializeValue(data.findFirst("enabled"), enabled)) return error;
    if (auto error = serde::deserializeValue(data.findFirst("zsort"), zsortRel)) return error;
    if (auto error = localTransform.deserializeFromFghj(data.findFirst("transform"))) return error;
    const auto lock = data.findFirst("lockToRoot");
    if (!lock.isMissing()) if (auto error = serde::deserializeValue(lock, lockToRoot)) return error;
    const auto pin = data.findFirst("pinToMesh");
    if (!pin.isMissing()) if (auto error = serde::deserializeValue(pin, pinToMesh)) return error;
    const auto childData = data.findFirst("children");
    // byElement is empty on missing, null and every non-array kind.
    if (!childData.isMissing() && childData.kind() == serde::JsonKind::Array) {
        for (const auto element : childData.elements()) {
            d_runtime::RetainedString type;
            if (auto error = serde::deserializeValue(element.findFirst("type"), type)) return error;
            if (!inHasNodeTypeRetained(type)) continue;
            auto child = inInstantiateNodeRetained(type, shared_from_this());
            if (auto error = d_runtime::ngRequireObject(child)->deserializeFromFghj(element)) return error;
        }
    }
    return {};
}

} // namespace nicxlive::core::nodes

