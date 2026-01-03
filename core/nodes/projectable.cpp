#include "projectable.hpp"

#include <algorithm>

#include "../puppet.hpp"

namespace nicxlive::core::nodes {

Projectable::Projectable(bool /*delegatedMode*/) {}

void Projectable::selfSort() {
    std::sort(subParts.begin(), subParts.end(), [](const std::shared_ptr<Part>& a, const std::shared_ptr<Part>& b) {
        if (!a || !b) return false;
        return a->zSort() > b->zSort();
    });
}

void Projectable::setIgnorePuppetRecurse(const std::shared_ptr<Node>& node, bool ignore) {
    if (!node) return;
    if (auto part = std::dynamic_pointer_cast<Part>(node)) {
        part->ignorePuppet = ignore;
        for (auto& child : part->childrenRef()) {
            setIgnorePuppetRecurse(child, ignore);
        }
    } else {
        for (auto& child : node->childrenRef()) {
            setIgnorePuppetRecurse(child, ignore);
        }
    }
}

void Projectable::setIgnorePuppet(bool ignore) {
    for (auto& child : childrenRef()) {
        setIgnorePuppetRecurse(child, ignore);
    }
    if (auto pup = puppetRef()) {
        pup->rescanNodes();
    }
}

void Projectable::scanPartsRecurse(const std::shared_ptr<Node>& node) {
    if (!node) return;
    auto proj = std::dynamic_pointer_cast<Projectable>(node);
    auto part = std::dynamic_pointer_cast<Part>(node);
    auto mask = std::dynamic_pointer_cast<Mask>(node);
    if (part && node.get() != this) {
        subParts.push_back(part);
        for (auto& c : part->childrenRef()) scanPartsRecurse(c);
    } else if (mask && node.get() != this) {
        maskParts.push_back(mask);
        for (auto& c : mask->childrenRef()) scanPartsRecurse(c);
    } else if ((!proj || node.get() == this) && node->enabled) {
        for (auto& c : node->childrenRef()) scanPartsRecurse(c);
    } else if (proj && node.get() != this) {
        proj->scanPartsRecurse(proj);
    }
}

void Projectable::drawSelf(bool isMask) {
    if (childrenRef().empty()) return;
    Part::drawSelf(isMask);
}

Transform Projectable::fullTransform() const {
    auto local = transform();
    if (lockToRootValue()) {
        if (auto pup = puppetRef()) {
            if (auto root = pup->root) {
                return local.calcOffset(root->transform());
            }
        }
    }
    if (auto p = parentPtr()) {
        auto pt = p->transform();
        return local.calcOffset(pt);
    }
    return local;
}

Mat4 Projectable::fullTransformMatrix() const { return fullTransform().toMat4(); }

Vec4 Projectable::boundsFromMatrix(const std::shared_ptr<Part>& child, const Mat4& matrix) const {
    float tx = matrix[0][3];
    float ty = matrix[1][3];
    Vec4 bounds{tx, ty, tx, ty};
    if (!child || child->mesh.vertices.empty()) return bounds;
    for (std::size_t i = 0; i < child->mesh.vertices.size(); ++i) {
        Vec2 local = child->mesh.vertices[i];
        if (i < child->deformation.size()) {
            local.x += child->deformation.x[i];
            local.y += child->deformation.y[i];
        }
        Vec3 res = matrix.transformPoint(Vec3{local.x, local.y, 0});
        bounds.x = std::min(bounds.x, res.x);
        bounds.y = std::min(bounds.y, res.y);
        bounds.z = std::max(bounds.z, res.x);
        bounds.w = std::max(bounds.w, res.y);
    }
    return bounds;
}

Vec4 Projectable::getChildrenBounds(bool forceUpdate) {
    if (useMaxChildrenBounds) {
        return maxChildrenBounds;
    }
    if (forceUpdate) {
        for (auto& p : subParts) {
            if (p) p->updateBounds();
        }
    }
    Vec4 bounds{0,0,0,0};
    bool hasBounds = false;
    Mat4 correction = Mat4::multiply(fullTransformMatrix(), Mat4::inverse(transform().toMat4()));
    for (auto& p : subParts) {
        if (!p) continue;
        if (auto b = p->bounds) {
            Vec4 pb{(*b)[0], (*b)[1], (*b)[2], (*b)[3]};
            if (!hasBounds) { bounds = pb; hasBounds = true; }
            else {
                bounds.x = std::min(bounds.x, pb.x);
                bounds.y = std::min(bounds.y, pb.y);
                bounds.z = std::max(bounds.z, pb.z);
                bounds.w = std::max(bounds.w, pb.w);
            }
        } else {
            auto childMatrix = Mat4::multiply(correction, p->transform().toMat4());
            auto childBounds = boundsFromMatrix(p, childMatrix);
            if (!hasBounds) { bounds = childBounds; hasBounds = true; }
            else {
                bounds.x = std::min(bounds.x, childBounds.x);
                bounds.y = std::min(bounds.y, childBounds.y);
                bounds.z = std::max(bounds.z, childBounds.z);
                bounds.w = std::max(bounds.w, childBounds.w);
            }
        }
    }
    if (!hasBounds) {
        auto t = transform();
        bounds = Vec4{t.translation.x, t.translation.y, t.translation.x, t.translation.y};
    }
    maxChildrenBounds = bounds;
    useMaxChildrenBounds = true;
    return bounds;
}

Vec4 Projectable::mergeBounds(const std::vector<Vec4>& bounds, const Vec4& origin) {
    if (bounds.empty()) return origin;
    Vec4 out = bounds.front();
    for (const auto& b : bounds) {
        out.x = std::min(out.x, b.x);
        out.y = std::min(out.y, b.y);
        out.z = std::max(out.z, b.z);
        out.w = std::max(out.w, b.w);
    }
    return out;
}

Vec2 Projectable::deformationTranslationOffset() const {
    if (deformation.size() == 0) return Vec2{0,0};
    Vec2 base{deformation.x[0], deformation.y[0]};
    const float eps = 0.0001f;
    for (std::size_t i = 0; i < deformation.size(); ++i) {
        if (std::abs(deformation.x[i] - base.x) > eps || std::abs(deformation.y[i] - base.y) > eps) {
            return Vec2{0,0};
        }
    }
    return base;
}

void Projectable::enableMaxChildrenBounds(const std::shared_ptr<Node>& target) {
    useMaxChildrenBounds = true;
    maxChildrenBounds = getChildrenBounds(true);
    if (auto d = std::dynamic_pointer_cast<Drawable>(target)) {
        if (d->bounds) {
            maxChildrenBounds.x = std::min(maxChildrenBounds.x, (*d->bounds)[0]);
            maxChildrenBounds.y = std::min(maxChildrenBounds.y, (*d->bounds)[1]);
            maxChildrenBounds.z = std::max(maxChildrenBounds.z, (*d->bounds)[2]);
            maxChildrenBounds.w = std::max(maxChildrenBounds.w, (*d->bounds)[3]);
        }
    }
}

void Projectable::invalidateChildrenBounds() { useMaxChildrenBounds = false; }

bool Projectable::createSimpleMesh() {
    auto bounds = getChildrenBounds();
    Vec2 size{bounds.z - bounds.x, bounds.w - bounds.y};
    if (size.x <= 0 || size.y <= 0) return false;
    Vec2 deformOffset = deformationTranslationOffset();
    autoResizedSize = size;
    // Build simple quad mesh
    MeshData data;
    data.vertices = {
        Vec2{bounds.x - deformOffset.x, bounds.y - deformOffset.y},
        Vec2{bounds.z - deformOffset.x, bounds.y - deformOffset.y},
        Vec2{bounds.z - deformOffset.x, bounds.w - deformOffset.y},
        Vec2{bounds.x - deformOffset.x, bounds.w - deformOffset.y}
    };
    data.uvs = {Vec2{0,0}, Vec2{1,0}, Vec2{1,1}, Vec2{0,1}};
    data.indices = {0,1,2, 0,2,3};
    mesh = data;
    updateVertices();
    return true;
}

bool Projectable::updateAutoResizedMeshOnce(bool& ran) {
    ran = true;
    return createSimpleMesh();
}

bool Projectable::updatePartMeshOnce(bool& ran) {
    ran = true;
    return createSimpleMesh();
}

bool Projectable::autoResizeMeshOnce(bool& ran) {
    ran = true;
    return createSimpleMesh();
}

bool Projectable::initTarget() {
    updateBounds();
    if (!bounds) return false;
    auto b = *bounds;
    Vec2 size{b[2]-b[0], b[3]-b[1]};
    if (size.x <= 0 || size.y <= 0) return false;
    texWidth = static_cast<uint32_t>(std::ceil(size.x)) + 1;
    texHeight = static_cast<uint32_t>(std::ceil(size.y)) + 1;
    Vec2 deformOffset = deformationTranslationOffset();
    auto t = transform();
    textureOffset = Vec2{(b[0]+b[2])/2.0f, (b[1]+b[3])/2.0f};
    textureOffset.x += deformOffset.x - t.translation.x;
    textureOffset.y += deformOffset.y - t.translation.y;
    textures = {};
    textures[0] = std::make_shared<Texture>(static_cast<int>(texWidth), static_cast<int>(texHeight));
    stencil = std::make_shared<Texture>(static_cast<int>(texWidth), static_cast<int>(texHeight), 4, true);
    reuseCachedTextureThisFrame = false;
    initialized = true;
    textureInvalidated = true;
    hasValidOffscreenContent = false;
    loggedFirstRenderAttempt = false;
    return true;
}

bool Projectable::updateDynamicRenderStateFlags() {
    static std::size_t frameCounter = 0;
    auto frameId = ++frameCounter;
    bool resized = false;
    if (deferredChanged) {
        bool ran = false;
        resized = updateAutoResizedMeshOnce(ran);
        deferredChanged = false;
        textureInvalidated = true;
        hasValidOffscreenContent = false;
        loggedFirstRenderAttempt = false;
    }
    if (boundsDirty) {
        createSimpleMesh();
        boundsDirty = false;
        textureInvalidated = true;
    }
    if (!initialized) {
        if (lastInitAttemptFrame == frameId) return false;
        lastInitAttemptFrame = frameId;
        if (!initTarget()) return false;
    }
    if (shouldUpdateVertices) {
        shouldUpdateVertices = false;
    }
    return true;
}

void Projectable::dynamicRenderBegin(core::RenderContext&) {
    reuseCachedTextureThisFrame = false;
    hasValidOffscreenContent = false;
}

void Projectable::dynamicRenderEnd(core::RenderContext&) {
    hasValidOffscreenContent = true;
}

void Projectable::enqueueRenderCommands(core::RenderContext&) {
    selfSort();
    Mat4 translate = Mat4::translation(Vec3{-textureOffset.x, -textureOffset.y, 0});
    Mat4 correction = Mat4::multiply(fullTransformMatrix(), Mat4::inverse(transform().toMat4()));
    Mat4 childBasis = Mat4::multiply(translate, Mat4::inverse(transform().toMat4()));
    queuedOffscreenParts.clear();
    for (auto& p : subParts) {
        if (!p) continue;
        Mat4 childMatrix = Mat4::multiply(correction, p->transform().toMat4());
        Mat4 finalMatrix = Mat4::multiply(childBasis, childMatrix);
        p->setOffscreenModelMatrix(finalMatrix);
        core::RenderContext dummyCtx{};
        p->runRenderTask(dummyCtx);
        queuedOffscreenParts.push_back(p);
    }
    for (auto& m : maskParts) {
        if (!m) continue;
        Mat4 maskMatrix = Mat4::multiply(correction, m->transform().toMat4());
        Mat4 finalMatrix = Mat4::multiply(translate, maskMatrix);
        m->setOffscreenModelMatrix(finalMatrix);
        m->renderMask(false);
        queuedOffscreenParts.push_back(m);
    }
}

void Projectable::runRenderTask(core::RenderContext& ctx) {
    if (!updateDynamicRenderStateFlags()) return;
    dynamicRenderBegin(ctx);
    enqueueRenderCommands(ctx);
    dynamicRenderEnd(ctx);
    // clear temporary offscreen matrices
    for (auto& d : queuedOffscreenParts) {
        if (!d) continue;
        if (auto p = std::dynamic_pointer_cast<Part>(d)) p->clearOffscreenModelMatrix();
        else if (auto m = std::dynamic_pointer_cast<Mask>(d)) m->clearOffscreenModelMatrix();
    }
    queuedOffscreenParts.clear();
}

void Projectable::runBeginTask(core::RenderContext& ctx) {
    Part::runBeginTask(ctx);
    useMaxChildrenBounds = false;
    ancestorChangeQueued = false;
    dynamicScopeActive = false;
    reuseCachedTextureThisFrame = false;
    subParts.clear();
    maskParts.clear();
    scanPartsRecurse(shared_from_this());
}

void Projectable::runPostTaskImpl(std::size_t priority, core::RenderContext& ctx) {
    Part::runPostTaskImpl(priority, ctx);
    if (priority == 0) {
        useMaxChildrenBounds = false;
    }
    if (priority == 1 && ancestorChangeQueued) {
        boundsDirty = true;
    }
}

void Projectable::notifyChange(const std::shared_ptr<Node>& target, NotifyReason reason) {
    ancestorChangeQueued = true;
    boundsDirty = true;
    Part::notifyChange(target, reason);
}

void Projectable::serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const {
    // Temporarily drop textures to mimic D behavior
    auto savedTextures = textures;
    auto savedStencil = stencil;
    const_cast<Projectable*>(this)->textures = {nullptr, nullptr, nullptr};
    const_cast<Projectable*>(this)->stencil.reset();
    Part::serializeSelfImpl(serializer, recursive, flags);
    if (has_flag(flags, SerializeNodeFlags::State)) {
        serializer.putKey("auto_resized");
        serializer.putValue(autoResizedMesh);
        serializer.putKey("textureOffset");
        serializer.putKey("x"); serializer.putValue(textureOffset.x);
        serializer.putKey("y"); serializer.putValue(textureOffset.y);
        serializer.putKey("boundsDirty");
        serializer.putValue(boundsDirty);
        serializer.putKey("textureInvalidated");
        serializer.putValue(textureInvalidated);
        serializer.putKey("deferred");
        serializer.putValue(deferred);
    }
    const_cast<Projectable*>(this)->textures = savedTextures;
    const_cast<Projectable*>(this)->stencil = savedStencil;
}

::nicxlive::core::serde::SerdeException Projectable::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    auto res = Part::deserializeFromFghj(data);
    textures = {};
    if (auto ar = data.get_optional<bool>("auto_resized")) {
        autoResizedMesh = *ar;
    } else if (!mesh.indices.empty()) {
        autoResizedMesh = false;
    } else {
        autoResizedMesh = true;
    }
    if (auto tx = data.get_optional<float>("textureOffset.x")) textureOffset.x = *tx;
    if (auto ty = data.get_optional<float>("textureOffset.y")) textureOffset.y = *ty;
    if (auto bd = data.get_optional<bool>("boundsDirty")) boundsDirty = *bd;
    if (auto ti = data.get_optional<bool>("textureInvalidated")) textureInvalidated = *ti;
    if (auto df = data.get_optional<int>("deferred")) deferred = *df;
    return res;
}

} // namespace nicxlive::core::nodes
