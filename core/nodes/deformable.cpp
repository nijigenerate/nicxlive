#include "deformable.hpp"
#include "../puppet.hpp"
#include "../render/profiler.hpp"
#include "../common/object_key_policy.hpp"
#include "../compat/static_error.hpp"

namespace nicxlive::core::nodes {
namespace {
// Independent deformable.d:26-28 module global: TLS, shared by base instances.
thread_local Vec2Array sourceDeformableDummy;

template<class Values> decltype(auto) sourceDeformableAt(Values& values,
    std::size_t index, unsigned line) {
    if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
        index, values.size(), d_runtime::RetainedString::fromStaticLiteral(
            "source/nijilive/core/nodes/deformable.d"), line);
    return values[index];
}

template <typename HookVector>
bool hasDeformHook(const HookVector& hooks, int stage, std::uintptr_t tag) {
    return std::any_of(hooks.begin(), hooks.end(), [&](const auto& hook) {
        return hook.stage == stage && hook.tag == tag;
    });
}

template <typename HookVector>
void upsertDeformHook(HookVector& hooks, const Node::DeformFilterHook& hook, bool prepend) {
    hooks.erase(std::remove_if(hooks.begin(), hooks.end(), [&](const auto& existing) {
        return existing.stage == hook.stage && existing.tag == hook.tag;
    }), hooks.end());
    if (prepend) {
        hooks.insert(hooks.begin(), hook);
    } else {
        hooks.push_back(hook);
    }
}

template <typename HookVector>
void eraseDeformHook(HookVector& hooks, int stage, std::uintptr_t tag) {
    hooks.erase(std::remove_if(hooks.begin(), hooks.end(), [&](const auto& hook) {
        return hook.stage == stage && hook.tag == tag;
    }), hooks.end());
}
} // namespace

void Deformation::update(const Vec2Array& points) {
    const auto input = points;
    auto independent = input.dup();
    std::destroy_at(std::addressof(vertexOffsets));
    std::construct_at(std::addressof(vertexOffsets), std::move(independent));
}

Deformation& Deformation::operator=(const Deformation& other) {
    const auto input = other;
    update(input.vertexOffsets);
    return *this;
}

Deformation& Deformation::operator=(Deformation&& other) {
    // Source opAssign accepts const Deformation, including temporary values.
    return operator=(static_cast<const Deformation&>(other));
}

Deformation::operator param::DeformSlot() const {
    return param::DeformSlot(vertexOffsets.dup());
}

Deformation Deformation::operator-() const {
    return deformation_source::ngNegate(*this);
}

Deformation Deformation::operator+(const Deformation& other) const {
    return deformation_source::ngBinary<deformation_source::Operation::Add>(*this, other);
}

Deformation Deformation::operator-(const Deformation& other) const {
    return deformation_source::ngBinary<deformation_source::Operation::Subtract>(*this, other);
}

Deformation Deformation::operator*(float s) const {
    return deformation_source::ngBinary<deformation_source::Operation::Multiply>(*this, s);
}

DeformationStack::DeformationStack(Deformable* owner) : owner_(owner) {
    retainedParent_ = retainParent();
}

DeformationStack::DeformationStack(std::shared_ptr<Deformable> owner)
    : owner_(owner.get()), retainedParent_(std::move(owner)) {}

DeformationStack::DeformationStack(const DeformationStack& other)
    : owner_(other.owner_), retainedParent_(other.retainParent()) {}

DeformationStack::DeformationStack(DeformationStack&& other)
    : DeformationStack(static_cast<const DeformationStack&>(other)) {}

DeformationStack& DeformationStack::operator=(const DeformationStack& other) {
    const auto container = retainContainer();
    auto parent = other.retainParent();
    auto* owner = other.owner_;
    owner_ = owner;
    if (owner == containingOwner_) parent.reset();
    retainedParent_ = std::move(parent);
    return *this;
}

DeformationStack& DeformationStack::operator=(DeformationStack&& other) {
    return operator=(static_cast<const DeformationStack&>(other));
}

std::shared_ptr<Deformable> DeformationStack::retainParent() const {
    if (retainedParent_) return retainedParent_;
    return owner_ ? std::static_pointer_cast<Deformable>(owner_->weak_from_this().lock()) : nullptr;
}

std::shared_ptr<Node> DeformationStack::retainContainer() const {
    return containingOwner_ ? containingOwner_->weak_from_this().lock() : nullptr;
}

void DeformationStack::preUpdate() {
    const auto container = retainContainer();
    const auto parent = retainParent();
    auto& values = d_runtime::ngRequireObject(owner_)->getDeformation();
    for (std::size_t i = 0; i < values.size(); ++i) {
        values.set(i, Vec2{0.0f, 0.0f});
    }
}

void DeformationStack::update() {
    const auto container = retainContainer();
    const auto parent = retainParent();
    // Source refreshDeform is final; its body still dispatches updateDeform.
    d_runtime::ngRequireObject(owner_)->Deformable::refreshDeform();
}

void DeformationStack::push(const Deformation& deform) {
    const auto& d = deform.vertexOffsets;
    if (!owner_ || owner_->deformation.size() != d.size()) return;
    owner_->deformation += d;
    float maxAbs = 0.0f;
    for (std::size_t i = 0; i < owner_->deformation.size(); ++i) {
        maxAbs = std::max(maxAbs, std::max(std::fabs(owner_->deformation.xAt(i)), std::fabs(owner_->deformation.yAt(i))));
    }
    if (maxAbs > 50.0f) {
        NJCX_DBG_LOG("[nicxlive][Deformable][StackLarge] node=%s uuid=%u maxAbs=%.6f first=(%.6f,%.6f)\n",
                     owner_->name.c_str(), owner_->uuid, maxAbs,
                     owner_->deformation.size() ? owner_->deformation.xAt(0) : 0.0f,
                     owner_->deformation.size() ? owner_->deformation.yAt(0) : 0.0f);
    }
    owner_->notifyDeformPushed(d);
}

void DeformationStack::push(Deformation& deform) {
    // defstack.d: the callback receives the original mutable value, not its array.
    const auto container = retainContainer();
    {
        const auto parent = retainParent();
        if (d_runtime::ngRequireObject(owner_)->getDeformation().size() != deform.vertexOffsets.size()) return;
    }
    {
        const auto parent = retainParent();
        auto& destination = d_runtime::ngRequireObject(owner_)->getDeformation();
        const auto offsets = deform.vertexOffsets;
        deformation_source::ngApplyArray(destination, offsets, deformation_source::Operation::Add);
    }
    {
        const auto parent = retainParent();
        d_runtime::ngRequireObject(owner_)->notifyDeformPushed(deform);
    }
}

Deformable::Deformable() {
    requirePreProcessTask();
    requirePostTask(0);
}

Deformable::Deformable(const std::shared_ptr<Node>& parent) {
    requirePreProcessTask();
    requirePostTask(0);
    setParent(parent);
}

Deformable::Deformable(uint32_t uuidVal, const std::shared_ptr<Node>& parent) {
    requirePreProcessTask();
    requirePostTask(0);
    uuid = uuidVal;
    setParent(parent);
}

Deformable::Deformable(Vec2Array verts)
    : vertices(std::move(verts)), deformStack(this, this) {
    deformation.resize(vertices.size());
    requirePreProcessTask();
    requirePostTask(0);
}

void Deformable::refresh() { updateVertices(); }
void Deformable::refreshDeform() { updateDeform(); }

Vec2Array& Deformable::getVertices() { return sourceDeformableDummy; }

void Deformable::updateBounds() {}

void Deformable::rebuffer(const Vec2Array& verts) {
    const Vec2Array source(verts);
    getVertices() = source;
    const auto count = source.size();
    getDeformation().resize(count);
}

void Deformable::pushDeformation(const Deformation& delta) {
    deformStack.push(delta);
}

void Deformable::updateDeform() {
    [[maybe_unused]] const auto lifetime = weak_from_this().lock();
    const auto deformationCount = getDeformation().size();
    const auto vertexCount = getVertices().size();
    if (deformationCount != vertexCount) {
        // Assignment evaluates the original RHS before the mutable LHS getter.
        const auto count = getVertices().size();
        getDeformation().resize(count);
        getDeformation().fill(Vec2{0, 0});
    }
}

void Deformable::remapDeformationBindings(const std::vector<std::size_t>& remap,
    const Vec2Array& replacement, std::size_t newLength) {
    // Legacy native ingress; the actual original body uses value descriptors.
    common::SharedArray<std::size_t> indices{std::vector<std::size_t>(remap)};
    remapDeformationBindings(indices, Vec2Array(replacement), newLength);
}

void Deformable::remapDeformationBindings(common::SharedArray<std::size_t> remap,
    Vec2Array replacement, std::size_t newLength) {
    [[maybe_unused]] const auto lifetime = weak_from_this().lock();
    using DeformationBase = core::param::ParameterBindingImpl<core::param::DeformSlot>;
    const auto parameters = d_runtime::ngRequireObject(puppetRef())->parameters;
    for (auto parameter : parameters) {
        if (!parameter) continue;
        const auto bindings = parameter->bindings;
        for (auto binding : bindings) {
            const auto target = d_runtime::ngRequireObject(binding)->getTarget().getResource();
            if (common::ngClassObjectIdentity(target) != dynamic_cast<const void*>(this)) continue;
            const auto deformBinding = std::dynamic_pointer_cast<core::param::DeformationParameterBinding>(binding);
            if (!deformBinding) continue;
            const auto xCount = deformBinding->values.size();
            for (std::size_t x = 0; x < xCount; ++x) {
                const auto yCount = sourceDeformableAt(deformBinding->values, x, 146).size();
                for (std::size_t y = 0; y < yCount; ++y) {
                    auto offsets = sourceDeformableAt(sourceDeformableAt(
                        deformBinding->values, x, 147), y, 147).vertexOffsets;
                    if (remap.size() == offsets.size() && !remap.empty()) {
                        Vec2Array reordered;
                        reordered.resize(remap.size());
                        const auto indices = remap;
                        for (std::size_t oldIndex = 0; oldIndex < indices.size(); ++oldIndex) {
                            const auto newIndex = indices[oldIndex];
                            const auto point = offsets[oldIndex];
                            reordered[newIndex] = point;
                        }
                        sourceDeformableAt(sourceDeformableAt(
                            deformBinding->values, x, 154), y, 154).vertexOffsets = reordered;
                    } else if (replacement.size() == newLength && newLength > 0) {
                        const auto independent = replacement.dup();
                        sourceDeformableAt(sourceDeformableAt(
                            deformBinding->values, x, 156), y, 156).vertexOffsets = independent;
                        auto& flags = deformBinding->DeformationBase::getIsSet();
                        sourceDeformableAt(sourceDeformableAt(flags, x, 157), y, 157) = true;
                    } else {
                        offsets.resize(newLength);
                        offsets.fill(Vec2{0, 0});
                        sourceDeformableAt(sourceDeformableAt(
                            deformBinding->values, x, 161), y, 161).vertexOffsets = offsets;
                        auto& flags = deformBinding->DeformationBase::getIsSet();
                        sourceDeformableAt(sourceDeformableAt(flags, x, 162), y, 162) = false;
                    }
                }
            }
        }
    }
}

void Deformable::preProcess() {
    const auto lifetime = weak_from_this().lock();
    if (preProcessed) return;
    preProcessed = true;
    const auto filters = preProcessFilters;
    for (const auto filter : filters) {
        const auto matrixOwner = overrideTransformMatrix;
        Mat4 matrix = matrixOwner ? matrixOwner->matrix : transform().toMat4();
        const auto receiver = shared_from_this();
        auto verticesValue = getVertices();
        auto deformationValue = getDeformation();
        auto result = filter.func(receiver, verticesValue, deformationValue, &matrix);
        [[maybe_unused]] const auto returnedMatrixOwner = overrideTransformMatrix;
        if (!std::get<0>(result).empty()) {
            const auto value = std::get<0>(result);
            getDeformation() = value;
        }
        if (auto* returnedMatrix = std::get<1>(result))
            overrideTransformMatrix = std::make_shared<MatrixHolder>(*returnedMatrix);
        if (std::get<2>(result)) notifyChange(shared_from_this());
    }
}

void Deformable::postProcess(int id) {
    const auto lifetime = weak_from_this().lock();
    if (postProcessed >= id) return;
    postProcessed = id;
    const auto filters = postProcessFilters;
    for (const auto filter : filters) {
        if (filter.stage != id) continue;
        const auto matrixOwner = overrideTransformMatrix;
        Mat4 matrix = matrixOwner ? matrixOwner->matrix : transform().toMat4();
        const auto receiver = shared_from_this();
        auto verticesValue = getVertices();
        auto deformationValue = getDeformation();
        auto result = filter.func(receiver, verticesValue, deformationValue, &matrix);
        [[maybe_unused]] const auto returnedMatrixOwner = overrideTransformMatrix;
        if (!std::get<0>(result).empty()) {
            const auto value = std::get<0>(result);
            getDeformation() = value;
        }
        if (auto* returnedMatrix = std::get<1>(result))
            overrideTransformMatrix = std::make_shared<MatrixHolder>(*returnedMatrix);
        if (std::get<2>(result)) notifyChange(shared_from_this());
    }
}

void Deformable::copyFrom(const Node& src, bool clone, bool deepCopy) {
    Node::copyFrom(src, clone, deepCopy);
}

void Deformable::runBeginTask(core::RenderContext& ctx) {
    deformStack.preUpdate();
    overrideTransformMatrix.reset();
    Node::runBeginTask(ctx);
}

void Deformable::runPreProcessTask(core::RenderContext& ctx) {
    auto scope = core::render::profileScope("Deformable.runPreProcessTask");
    Node::runPreProcessTask(ctx);
    deformStack.update();
}

void Deformable::runPostTaskImpl(std::size_t priority, core::RenderContext& ctx) {
    Node::runPostTaskImpl(priority, ctx);
    updateDeform();
}

void Deformable::notifyDeformPushed(const Vec2Array& deform) { onDeformPushed(deform); }
void Deformable::notifyDeformPushed(Deformation& deform) { onDeformPushed(deform); }

bool Deformable::hasDeformPreProcessFilter(int stage, std::uintptr_t tag) const {
    return hasDeformHook(deformPreProcessFilters, stage, tag);
}

bool Deformable::hasDeformPostProcessFilter(int stage, std::uintptr_t tag) const {
    return hasDeformHook(deformPostProcessFilters, stage, tag);
}

void Deformable::upsertDeformPreProcessFilter(Node::DeformFilterHook hook, bool prepend) {
    requirePreProcessTask();
    upsertDeformHook(deformPreProcessFilters, hook, prepend);
}

void Deformable::upsertDeformPostProcessFilter(Node::DeformFilterHook hook, bool prepend) {
    requirePostTask(static_cast<std::size_t>(std::max(hook.stage, 0)));
    upsertDeformHook(deformPostProcessFilters, hook, prepend);
}

void Deformable::removeDeformPreProcessFilter(int stage, std::uintptr_t tag) {
    eraseDeformHook(deformPreProcessFilters, stage, tag);
}

void Deformable::removeDeformPostProcessFilter(int stage, std::uintptr_t tag) {
    eraseDeformHook(deformPostProcessFilters, stage, tag);
}

bool areDeformationNodesCompatible(const std::shared_ptr<Node>& lhs, const std::shared_ptr<Node>& rhs) {
    auto dl = std::dynamic_pointer_cast<Deformable>(lhs);
    auto dr = std::dynamic_pointer_cast<Deformable>(rhs);
    if (!dl || !dr) return false;
    return dl->getVertices().size() == dr->getVertices().size();
}

} // namespace nicxlive::core::nodes
