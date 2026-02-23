#include "deformable.hpp"
#include "../puppet.hpp"
#include "../render/shared_deform_buffer.hpp"
#include <cstdio>
#include <cmath>

namespace nicxlive::core::nodes {
namespace {
std::vector<Vec2> toStdVec(const Vec2Array& src) {
    std::vector<Vec2> out;
    out.reserve(src.size());
    for (std::size_t i = 0; i < src.size(); ++i) out.push_back(src[i]);
    return out;
}

void fromStdVec(Vec2Array& dst, const std::vector<Vec2>& src) {
    dst.resize(src.size());
    for (std::size_t i = 0; i < src.size(); ++i) {
        dst.set(i, src[i]);
    }
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
    auto n = std::min(vertexOffsets.size(), other.vertexOffsets.size());
    out.vertexOffsets.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.vertexOffsets.xAt(i) = vertexOffsets.xAt(i) + other.vertexOffsets.xAt(i);
        out.vertexOffsets.yAt(i) = vertexOffsets.yAt(i) + other.vertexOffsets.yAt(i);
    }
    return out;
}

Deformation Deformation::operator-(const Deformation& other) const {
    Deformation out;
    auto n = std::min(vertexOffsets.size(), other.vertexOffsets.size());
    out.vertexOffsets.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.vertexOffsets.xAt(i) = vertexOffsets.xAt(i) - other.vertexOffsets.xAt(i);
        out.vertexOffsets.yAt(i) = vertexOffsets.yAt(i) - other.vertexOffsets.yAt(i);
    }
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
        std::fprintf(stderr,
                     "[nicxlive][Deformable][StackLarge] node=%s uuid=%u maxAbs=%.6f first=(%.6f,%.6f)\n",
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
    if (preProcessed) return;
    preProcessed = true;
    for (const auto& hook : preProcessFilters) {
        Mat4 matrix = overrideTransformMatrix ? *overrideTransformMatrix : transform().toMat4();
        auto verts = toStdVec(vertices);
        auto deform = toStdVec(deformation);
        auto result = hook.func(shared_from_this(), verts, deform, &matrix);
        const auto& newDeform = std::get<0>(result);
        const auto& newMat = std::get<1>(result);
        bool notify = std::get<2>(result);
        if (!newDeform.empty()) {
            fromStdVec(deformation, newDeform);
            ::nicxlive::core::render::sharedDeformMarkDirty();
            float maxAbs = 0.0f;
            for (std::size_t i = 0; i < deformation.size(); ++i) {
                maxAbs = std::max(maxAbs, std::max(std::fabs(deformation.xAt(i)), std::fabs(deformation.yAt(i))));
            }
            if (maxAbs > 50.0f) {
                std::fprintf(stderr,
                             "[nicxlive][Deformable][PreFilterLarge] node=%s uuid=%u stage=%zu maxAbs=%.6f first=(%.6f,%.6f)\n",
                             name.c_str(), uuid, hook.stage, maxAbs,
                             deformation.size() ? deformation.xAt(0) : 0.0f,
                             deformation.size() ? deformation.yAt(0) : 0.0f);
            }
        }
        if (newMat.has_value()) {
            overrideTransformMatrix = newMat;
        }
        if (notify) {
            notifyChange(shared_from_this());
        }
    }
}

void Deformable::postProcess(int id) {
    if (postProcessed >= id) return;
    postProcessed = id;
    for (const auto& hook : postProcessFilters) {
        if (hook.stage != id) continue;
        Mat4 matrix = overrideTransformMatrix ? *overrideTransformMatrix : transform().toMat4();
        auto verts = toStdVec(vertices);
        auto deform = toStdVec(deformation);
        auto result = hook.func(shared_from_this(), verts, deform, &matrix);
        const auto& newDeform = std::get<0>(result);
        const auto& newMat = std::get<1>(result);
        bool notify = std::get<2>(result);
        if (!newDeform.empty()) {
            fromStdVec(deformation, newDeform);
            ::nicxlive::core::render::sharedDeformMarkDirty();
            float maxAbs = 0.0f;
            for (std::size_t i = 0; i < deformation.size(); ++i) {
                maxAbs = std::max(maxAbs, std::max(std::fabs(deformation.xAt(i)), std::fabs(deformation.yAt(i))));
            }
            if (maxAbs > 50.0f) {
                std::fprintf(stderr,
                             "[nicxlive][Deformable][PostFilterLarge] node=%s uuid=%u stage=%zu maxAbs=%.6f first=(%.6f,%.6f)\n",
                             name.c_str(), uuid, hook.stage, maxAbs,
                             deformation.size() ? deformation.xAt(0) : 0.0f,
                             deformation.size() ? deformation.yAt(0) : 0.0f);
            }
        }
        if (newMat.has_value()) {
            overrideTransformMatrix = newMat;
        }
        if (notify) {
            notifyChange(shared_from_this());
        }
    }
}

void Deformable::runBeginTask(core::RenderContext& ctx) {
    deformStack.preUpdate();
    overrideTransformMatrix.reset();
    Node::runBeginTask(ctx);
}

void Deformable::runPreProcessTask(core::RenderContext& ctx) {
    Node::runPreProcessTask(ctx);
    deformStack.update();
}

void Deformable::runPostTaskImpl(std::size_t priority, core::RenderContext& ctx) {
    Node::runPostTaskImpl(priority, ctx);
    updateDeform();
}

void Deformable::notifyDeformPushed(const Vec2Array& deform) { onDeformPushed(deform); }

bool areDeformationNodesCompatible(const std::shared_ptr<Node>& lhs, const std::shared_ptr<Node>& rhs) {
    auto dl = std::dynamic_pointer_cast<Deformable>(lhs);
    auto dr = std::dynamic_pointer_cast<Deformable>(rhs);
    if (!dl || !dr) return false;
    return dl->vertices.size() == dr->vertices.size();
}

} // namespace nicxlive::core::nodes
