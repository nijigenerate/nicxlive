#include "deformable.hpp"
#include "../puppet.hpp"
#include "../debug_log.hpp"
#include "../render/profiler.hpp"
#include <cstdio>
#include <cmath>

namespace nicxlive::core::nodes {
namespace {
const char* traceDeformChainTarget() {
    static const char* cached = nullptr;
    static bool init = false;
    if (!init) {
        cached = std::getenv("NJCX_TRACE_DEFORM_CHAIN");
        init = true;
    }
    return cached;
}

float maxAbsVec2Array(const Vec2Array& arr) {
    float maxAbs = 0.0f;
    for (std::size_t i = 0; i < arr.size(); ++i) {
        maxAbs = std::max(maxAbs, std::max(std::fabs(arr.xAt(i)), std::fabs(arr.yAt(i))));
    }
    return maxAbs;
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

void Deformation::update(const Vec2Array& points) { vertexOffsets = points; }

Deformation Deformation::operator-() const {
    Deformation out;
    out.vertexOffsets = vertexOffsets;
    for (std::size_t i = 0; i < out.vertexOffsets.size(); ++i) {
        out.vertexOffsets.xAt(i) *= -1.0f;
        out.vertexOffsets.yAt(i) *= -1.0f;
    }
    return out;
}

Deformation Deformation::operator+(const Deformation& other) const {
    Deformation out;
    out.vertexOffsets = vertexOffsets;
    out.vertexOffsets += other.vertexOffsets;
    return out;
}

Deformation Deformation::operator-(const Deformation& other) const {
    Deformation out;
    out.vertexOffsets = vertexOffsets;
    out.vertexOffsets -= other.vertexOffsets;
    return out;
}

Deformation Deformation::operator*(float s) const {
    Deformation out;
    out.vertexOffsets = vertexOffsets;
    for (std::size_t i = 0; i < out.vertexOffsets.size(); ++i) {
        out.vertexOffsets.xAt(i) *= s;
        out.vertexOffsets.yAt(i) *= s;
    }
    return out;
}

DeformationStack::DeformationStack(Deformable* owner) : owner_(owner) {}

void DeformationStack::preUpdate() {
    if (!owner_) return;
    for (std::size_t i = 0; i < owner_->deformation.size(); ++i) {
        owner_->deformation.set(i, Vec2{0.0f, 0.0f});
    }
}

void DeformationStack::update() {
    // D parity: update just normalizes deformation buffer length.
    if (!owner_) return;
    owner_->refreshDeform();
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
    : vertices(std::move(verts)), deformation(vertices.size()), deformStack(this) {
    requirePreProcessTask();
    requirePostTask(0);
}

void Deformable::refresh() { updateVertices(); }
void Deformable::refreshDeform() { updateDeform(); }

void Deformable::updateBounds() {}

void Deformable::rebuffer(const Vec2Array& verts) {
    vertices = verts;
    deformation.resize(vertices.size());
}

void Deformable::pushDeformation(const Deformation& delta) {
    deformStack.push(delta);
}

bool Deformable::mustPropagate() const { return true; }

void Deformable::updateDeform() {
    if (deformation.size() != vertices.size()) {
        deformation.resize(vertices.size());
        deformation.fill(Vec2{0.0f, 0.0f});
    }
}

void Deformable::remapDeformationBindings(const std::vector<std::size_t>& remap, const Vec2Array& replacement, std::size_t newLength) {
    auto pup = puppetRef();
    if (!pup) return;
    for (auto& param : pup->parameters) {
        if (!param) continue;
        auto deformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(param->getBinding(shared_from_this(), "deform"));
        if (!deformBinding) continue;
        deformBinding->remapOffsets(remap, replacement, newLength);
    }
}

void Deformable::preProcess() {
    auto scope = core::render::profileScope("Deformable.preProcess");
    if (preProcessed) return;
    Node::preProcess();
    const char* traceTarget = traceDeformChainTarget();
    const bool traceThisNode = traceTarget && name.find(traceTarget) != std::string::npos;
    bool anyNotify = false;
    for (const auto& hook : deformPreProcessFilters) {
        const float beforeAbs = traceThisNode ? maxAbsVec2Array(deformation) : 0.0f;
        Mat4 matrix = overrideTransformMatrix ? *overrideTransformMatrix : transform().toMat4();
        auto result = hook.func(shared_from_this(), vertices, deformation, &matrix);
        if (traceThisNode) {
            const float resultAbs = maxAbsVec2Array(deformation);
            NJCX_DBG_LOG("[nicxlive][DeformChain][Pre] node=%s uuid=%u tag=%zu stage=%d before=%.6f result=%.6f resultSize=%zu notify=%d\n",
                         name.c_str(),
                         uuid,
                         static_cast<std::size_t>(hook.tag),
                         hook.stage,
                         beforeAbs,
                         resultAbs,
                         deformation.size(),
                         result.changed ? 1 : 0);
        }
        if (result.transform.has_value()) {
            overrideTransformMatrix = *result.transform;
        }
        if (result.changed) {
            anyNotify = true;
        }
        if (traceThisNode) {
            const float afterAbs = maxAbsVec2Array(deformation);
            NJCX_DBG_LOG("[nicxlive][DeformChain][PreAfter] node=%s uuid=%u after=%.6f size=%zu\n",
                         name.c_str(),
                         uuid,
                         afterAbs,
                         deformation.size());
        }
    }
    if (anyNotify) {
        notifyChange(shared_from_this());
    }
}

void Deformable::postProcess(int id) {
    if (postProcessed >= id) return;
    Node::postProcess(id);
    bool anyNotify = false;
    for (const auto& hook : deformPostProcessFilters) {
        if (hook.stage != id) continue;
        Mat4 matrix = overrideTransformMatrix ? *overrideTransformMatrix : transform().toMat4();
        auto result = hook.func(shared_from_this(), vertices, deformation, &matrix);
        if (result.transform.has_value()) {
            overrideTransformMatrix = *result.transform;
        }
        if (result.changed) {
            anyNotify = true;
        }
    }
    if (anyNotify) {
        notifyChange(shared_from_this());
    }
}

void Deformable::copyFrom(const Node& src, bool clone, bool deepCopy) {
    Node::copyFrom(src, clone, deepCopy);
    if (auto deformable = dynamic_cast<const Deformable*>(&src)) {
        deformPreProcessFilters = deformable->deformPreProcessFilters;
        deformPostProcessFilters = deformable->deformPostProcessFilters;
    }
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
    return dl->vertices.size() == dr->vertices.size();
}

} // namespace nicxlive::core::nodes
