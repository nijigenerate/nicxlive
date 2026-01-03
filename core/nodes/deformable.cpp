#include "deformable.hpp"
#include "../puppet.hpp"

namespace nicxlive::core::nodes {

void Deformation::update(const Vec2Array& points) { vertexOffsets = points; }

Deformation Deformation::operator-() const {
    Deformation out;
    out.vertexOffsets = vertexOffsets;
    for (std::size_t i = 0; i < out.vertexOffsets.size(); ++i) {
        out.vertexOffsets.x[i] *= -1.0f;
        out.vertexOffsets.y[i] *= -1.0f;
    }
    return out;
}

Deformation Deformation::operator+(const Deformation& other) const {
    Deformation out;
    auto n = std::min(vertexOffsets.size(), other.vertexOffsets.size());
    out.vertexOffsets.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.vertexOffsets.x[i] = vertexOffsets.x[i] + other.vertexOffsets.x[i];
        out.vertexOffsets.y[i] = vertexOffsets.y[i] + other.vertexOffsets.y[i];
    }
    return out;
}

Deformation Deformation::operator-(const Deformation& other) const {
    Deformation out;
    auto n = std::min(vertexOffsets.size(), other.vertexOffsets.size());
    out.vertexOffsets.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.vertexOffsets.x[i] = vertexOffsets.x[i] - other.vertexOffsets.x[i];
        out.vertexOffsets.y[i] = vertexOffsets.y[i] - other.vertexOffsets.y[i];
    }
    return out;
}

Deformation Deformation::operator*(float s) const {
    Deformation out;
    out.vertexOffsets = vertexOffsets;
    for (std::size_t i = 0; i < out.vertexOffsets.size(); ++i) {
        out.vertexOffsets.x[i] *= s;
        out.vertexOffsets.y[i] *= s;
    }
    return out;
}

DeformationStack::DeformationStack(Deformable* owner) : owner_(owner) {}

void DeformationStack::preUpdate() {
    if (owner_) {
        owner_->deformation.resize(owner_->vertices.size());
        std::fill(owner_->deformation.x.begin(), owner_->deformation.x.end(), 0.0f);
        std::fill(owner_->deformation.y.begin(), owner_->deformation.y.end(), 0.0f);
    }
    pending_.clear();
}

void DeformationStack::update() {
    // Apply pending deformation to owner deformation buffer
    if (!owner_) return;
    if (owner_->deformation.size() < pending_.size()) {
        owner_->deformation.resize(pending_.size());
    }
    for (std::size_t i = 0; i < pending_.size(); ++i) {
        owner_->deformation.x[i] += pending_[i].x;
        owner_->deformation.y[i] += pending_[i].y;
    }
    pending_.clear();
}

void DeformationStack::push(const Deformation& deform) {
    const auto& d = deform.vertexOffsets;
    if (!owner_ || owner_->deformation.size() != d.size()) return;
    if (pending_.size() < d.size()) pending_.resize(d.size());
    for (std::size_t i = 0; i < d.size(); ++i) {
        pending_.x[i] += d.x[i];
        pending_.y[i] += d.y[i];
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
        std::fill(deformation.x.begin(), deformation.x.end(), 0.0f);
        std::fill(deformation.y.begin(), deformation.y.end(), 0.0f);
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

} // namespace nicxlive::core::nodes
