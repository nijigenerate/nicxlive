#include "projectable.hpp"

#include <algorithm>
#include <set>

#include "../puppet.hpp"

namespace nicxlive::core::nodes {
namespace {
std::size_t gProjectableFrameCounter = 0;
}

std::size_t advanceProjectableFrame() {
    return ++gProjectableFrameCounter;
}

std::size_t currentProjectableFrame() {
    return gProjectableFrameCounter;
}

static bool hasProjectableAncestor(const std::shared_ptr<Projectable>& node) {
    auto cur = node ? node->parentPtr() : nullptr;
    while (cur) {
        if (std::dynamic_pointer_cast<Projectable>(cur)) return true;
        cur = cur->parentPtr();
    }
    return false;
}

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

void Projectable::scanParts() {
    subParts.clear();
    maskParts.clear();
    for (auto& child : childrenRef()) {
        scanPartsRecurse(child);
    }
    invalidateChildrenBounds();
    boundsDirty = true;
}

void Projectable::scanSubParts(const std::vector<std::shared_ptr<Node>>& childNodes) {
    subParts.clear();
    maskParts.clear();
    for (auto& child : childNodes) {
        scanPartsRecurse(child);
    }
    invalidateChildrenBounds();
    boundsDirty = true;
}

void Projectable::scanPartsRecurse(const std::shared_ptr<Node>& node) {
    if (!node) return;
    auto proj = std::dynamic_pointer_cast<Projectable>(node);
    auto part = std::dynamic_pointer_cast<Part>(node);
    auto mask = std::dynamic_pointer_cast<Mask>(node);
    if (part && node.get() != this) {
        subParts.push_back(part);
        if (mask) {
            maskParts.push_back(mask);
        }
        if (!proj) {
            for (auto& c : part->childrenRef()) scanPartsRecurse(c);
        } else {
            proj->scanParts();
        }
    } else if (mask && node.get() != this) {
        maskParts.push_back(mask);
        for (auto& c : mask->childrenRef()) scanPartsRecurse(c);
    } else if ((!proj || node.get() == this) && node->enabled) {
        for (auto& c : node->childrenRef()) scanPartsRecurse(c);
    } else if (proj && node.get() != this) {
        proj->scanParts();
    }
}

void Projectable::drawSelf(bool isMask) {
    if (childrenRef().empty()) return;
    Part::drawSelf(isMask);
}

Transform Projectable::fullTransform() const {
    auto self = const_cast<Projectable*>(this);
    self->localTransform.update();
    self->offsetTransform.update();
    Transform base = self->localTransform.calcOffset(self->offsetTransform);
    if (lockToRootValue()) {
        if (auto pup = puppetRef()) {
            if (auto root = pup->root) {
                return base.calcOffset(root->localTransform);
            }
            return base.calcOffset(Transform{Vec3{0.0f, 0.0f, 0.0f}});
        }
        return base;
    }
    if (auto p = parentPtr()) {
        if (auto proj = std::dynamic_pointer_cast<Projectable>(p)) {
            return base.calcOffset(proj->fullTransform());
        }
        return base.calcOffset(p->transform());
    }
    return base;
}

Transform Projectable::transform() {
    auto trans = Part::transform();
    if (autoResizedMesh) {
        trans.rotation = Vec3{0, 0, 0};
        trans.scale = Vec2{1, 1};
        trans.update();
    }
    return trans;
}

Transform Projectable::transform() const {
    auto trans = Part::transform();
    if (autoResizedMesh) {
        trans.rotation = Vec3{0, 0, 0};
        trans.scale = Vec2{1, 1};
        trans.update();
    }
    return trans;
}

Mat4 Projectable::fullTransformMatrix() const { return fullTransform().toMat4(); }

Vec4 Projectable::boundsFromMatrix(const std::shared_ptr<Part>& child, const Mat4& matrix) const {
    float tx = matrix[0][3];
    float ty = matrix[1][3];
    Vec4 bounds{tx, ty, tx, ty};
    if (!child || child->vertices.size() == 0) return bounds;
    for (std::size_t i = 0; i < child->vertices.size(); ++i) {
        Vec2 local = child->vertices[i];
        if (i < child->deformation.size()) {
            auto d = child->deformation[i];
            local.x += d.x;
            local.y += d.y;
        }
        Vec3 res = matrix.transformPoint(Vec3{local.x, local.y, 0});
        bounds.x = std::min(bounds.x, res.x);
        bounds.y = std::min(bounds.y, res.y);
        bounds.z = std::max(bounds.z, res.x);
        bounds.w = std::max(bounds.w, res.y);
    }
    return bounds;
}

bool Projectable::detectAncestorTransformChange(std::size_t frameId) {
    if (lastAncestorTransformCheckFrame == frameId) {
        return false;
    }
    lastAncestorTransformCheckFrame = frameId;
    auto full = fullTransform();
    if (!hasCachedAncestorTransform) {
        prevTranslation = full.translation;
        prevRotation = full.rotation;
        prevScale = full.scale;
        hasCachedAncestorTransform = true;
        return false;
    }
    constexpr float TransformEpsilon = 0.0001f;
    const bool changed =
        std::abs(full.translation.x - prevTranslation.x) > TransformEpsilon ||
        std::abs(full.translation.y - prevTranslation.y) > TransformEpsilon ||
        std::abs(full.translation.z - prevTranslation.z) > TransformEpsilon ||
        std::abs(full.rotation.x - prevRotation.x) > TransformEpsilon ||
        std::abs(full.rotation.y - prevRotation.y) > TransformEpsilon ||
        std::abs(full.rotation.z - prevRotation.z) > TransformEpsilon ||
        std::abs(full.scale.x - prevScale.x) > TransformEpsilon ||
        std::abs(full.scale.y - prevScale.y) > TransformEpsilon;
    if (changed) {
        prevTranslation = full.translation;
        prevRotation = full.rotation;
        prevScale = full.scale;
    }
    return changed;
}

Vec4 Projectable::getChildrenBounds(bool forceUpdate) {
    constexpr std::size_t MaxBoundsResetInterval = 120;
    auto frameId = currentProjectableFrame();
    if (useMaxChildrenBounds) {
        if (frameId - maxBoundsStartFrame < MaxBoundsResetInterval) {
            return maxChildrenBounds;
        }
        useMaxChildrenBounds = false;
    }
    if (forceUpdate) {
        for (auto& p : subParts) {
            if (p) p->updateBounds();
        }
    }
    Vec4 bounds{0,0,0,0};
    bool hasBounds = false;
    Mat4 correction = Mat4::multiply(fullTransformMatrix(), Mat4::inverse(transform().toMat4()));
    auto boundsFinite = [](const Vec4& b) {
        return std::isfinite(b.x) && std::isfinite(b.y) && std::isfinite(b.z) && std::isfinite(b.w);
    };
    for (auto& p : subParts) {
        if (!p) continue;
        if (auto b = p->bounds; b && boundsFinite(Vec4{(*b)[0], (*b)[1], (*b)[2], (*b)[3]})) {
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
    maxBoundsStartFrame = frameId;
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
    Vec2 base = deformation[0];
    const float eps = 0.0001f;
    for (std::size_t i = 0; i < deformation.size(); ++i) {
        auto d = deformation[i];
        if (std::abs(d.x - base.x) > eps || std::abs(d.y - base.y) > eps) {
            return Vec2{0,0};
        }
    }
    return base;
}

void Projectable::enableMaxChildrenBounds(const std::shared_ptr<Node>& target) {
    auto targetDrawable = std::dynamic_pointer_cast<Drawable>(target);
    if (targetDrawable) {
        targetDrawable->updateBounds();
    }

    auto frameId = currentProjectableFrame();
    maxChildrenBounds = getChildrenBounds(false);
    useMaxChildrenBounds = true;
    maxBoundsStartFrame = frameId;

    if (targetDrawable) {
        bool hasFiniteBounds = false;
        Vec4 b{};
        if (targetDrawable->bounds) {
            b = Vec4{
                (*targetDrawable->bounds)[0],
                (*targetDrawable->bounds)[1],
                (*targetDrawable->bounds)[2],
                (*targetDrawable->bounds)[3]
            };
            hasFiniteBounds = std::isfinite(b.x) && std::isfinite(b.y) &&
                std::isfinite(b.z) && std::isfinite(b.w);
        }

        if (!hasFiniteBounds) {
            if (auto targetPart = std::dynamic_pointer_cast<Part>(targetDrawable)) {
                auto correction = Mat4::multiply(fullTransformMatrix(), Mat4::inverse(transform().toMat4()));
                b = boundsFromMatrix(targetPart, Mat4::multiply(correction, targetPart->transform().toMat4()));
                hasFiniteBounds = true;
            } else {
                return;
            }
        }

        maxChildrenBounds.x = std::min(maxChildrenBounds.x, b.x);
        maxChildrenBounds.y = std::min(maxChildrenBounds.y, b.y);
        maxChildrenBounds.z = std::max(maxChildrenBounds.z, b.z);
        maxChildrenBounds.w = std::max(maxChildrenBounds.w, b.w);
    }
}

void Projectable::invalidateChildrenBounds() { useMaxChildrenBounds = false; }

bool Projectable::createSimpleMesh() {
    auto bounds = getChildrenBounds();
    Vec2 size{bounds.z - bounds.x, bounds.w - bounds.y};
    if (size.x <= 0 || size.y <= 0) return false;
    Vec2 deformOffset = deformationTranslationOffset();
    Vec2 origSize = shouldUpdateVertices
        ? autoResizedSize
        : ((textures.size() > 0 && textures[0]) ? Vec2{static_cast<float>(textures[0]->width()), static_cast<float>(textures[0]->height())} : Vec2{0, 0});
    bool resizing = false;
    if (forceResize) {
        resizing = true;
        forceResize = false;
    } else {
        if (static_cast<int>(origSize.x) > static_cast<int>(size.x)) {
            float diff = (origSize.x - size.x) / 2.0f;
            bounds.z += diff;
            bounds.x -= diff;
        } else if (static_cast<int>(size.x) > static_cast<int>(origSize.x)) {
            resizing = true;
        }
        if (static_cast<int>(origSize.y) > static_cast<int>(size.y)) {
            float diff = (origSize.y - size.y) / 2.0f;
            bounds.w += diff;
            bounds.y -= diff;
        } else if (static_cast<int>(size.y) > static_cast<int>(origSize.y)) {
            resizing = true;
        }
    }

    auto originOffset = transform().translation;
    originOffset.x += deformOffset.x;
    originOffset.y += deformOffset.y;
    auto makeVert = [&](float x, float y) {
        return Vec2{x + deformOffset.x - originOffset.x, y + deformOffset.y - originOffset.y};
    };
    std::vector<Vec2> vertexArray{
        makeVert(bounds.x, bounds.y),
        makeVert(bounds.x, bounds.w),
        makeVert(bounds.z, bounds.y),
        makeVert(bounds.z, bounds.w)};

    if (resizing) {
        MeshData data;
        data.vertices = vertexArray;
        data.uvs = {Vec2{0, 0}, Vec2{0, 1}, Vec2{1, 0}, Vec2{1, 1}};
        data.indices = {0, 1, 2, 2, 1, 3};
        data.origin = Vec2{0, 0};
        data.gridAxes.clear();
        Part::rebuffer(data);
        shouldUpdateVertices = true;
        autoResizedSize = Vec2{bounds.z - bounds.x, bounds.w - bounds.y};
        textureOffset = Vec2{(bounds.x + bounds.z) / 2.0f + deformOffset.x - originOffset.x,
                             (bounds.y + bounds.w) / 2.0f + deformOffset.y - originOffset.y};
    } else {
        Vec2 newTextureOffset{(bounds.x + bounds.z) / 2.0f + deformOffset.x - originOffset.x,
                              (bounds.y + bounds.w) / 2.0f + deformOffset.y - originOffset.y};
        constexpr float TextureOffsetEpsilon = 0.001f;
        bool offsetChanged = std::abs(newTextureOffset.x - textureOffset.x) > TextureOffsetEpsilon ||
                             std::abs(newTextureOffset.y - textureOffset.y) > TextureOffsetEpsilon;
        if (offsetChanged) {
            textureInvalidated = true;
            mesh->vertices = vertexArray;
            mesh->indices = {0, 1, 2, 2, 1, 3};
            shouldUpdateVertices = true;
            autoResizedSize = Vec2{bounds.z - bounds.x, bounds.w - bounds.y};
            updateVertices();
            textureOffset = newTextureOffset;
        }
    }
    return resizing;
}

bool Projectable::updateAutoResizedMeshOnce(bool& ran) {
    if (!autoResizedMesh) {
        ran = false;
        return false;
    }
    ran = true;
    return createSimpleMesh();
}

bool Projectable::updatePartMeshOnce(bool& ran) {
    if (!autoResizedMesh) {
        ran = false;
        return false;
    }
    ran = true;
    return createSimpleMesh();
}

bool Projectable::autoResizeMeshOnce(bool& ran) {
    if (!autoResizedMesh) {
        ran = false;
        return false;
    }
    ran = true;
    return createSimpleMesh();
}

::nicxlive::core::DynamicCompositePass Projectable::prepareDynamicCompositePass() {
    ::nicxlive::core::DynamicCompositePass pass;
    if (textures.empty() || !textures[0]) return pass;
    if (!offscreenSurface) {
        offscreenSurface = std::make_shared<::nicxlive::core::DynamicCompositeSurface>();
    }
    std::size_t count = 0;
    for (std::size_t i = 0; i < offscreenSurface->textures.size(); ++i) {
        auto tex = i < textures.size() ? textures[i] : nullptr;
        offscreenSurface->textures[i] = tex;
        if (tex) count = i + 1;
    }
    offscreenSurface->textureCount = count;
    offscreenSurface->stencil = stencil;
    pass.surface = offscreenSurface;
    pass.textures.assign(textures.begin(), textures.end());
    pass.stencil = stencil;
    pass.scale = Vec2{transform().scale.x, transform().scale.y};
    pass.rotationZ = transform().rotation.z;
    pass.autoScaled = false;
    return pass;
}

bool Projectable::initTarget() {
    auto prevTexture = textures[0];
    auto prevStencil = stencil;

    updateBounds();
    std::array<float, 4> b{};
    bool hasBounds = false;
    if (bounds) {
        b = *bounds;
        hasBounds = std::isfinite(b[0]) && std::isfinite(b[1]) && std::isfinite(b[2]) && std::isfinite(b[3]);
    }
    if (!hasBounds) {
        auto t = transform();
        float minX = t.translation.x;
        float minY = t.translation.y;
        float maxX = t.translation.x;
        float maxY = t.translation.y;
        bool seeded = false;
        Mat4 matrix = getDynamicMatrix();
        for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
            Vec2 pos = mesh->vertices[i];
            if (i < deformation.size()) {
                auto d = deformation[i];
                pos.x += d.x;
                pos.y += d.y;
            }
            Vec3 v = matrix.transformPoint(Vec3{pos.x, pos.y, 0.0f});
            if (!seeded) {
                minX = maxX = v.x;
                minY = maxY = v.y;
                seeded = true;
            } else {
                minX = std::min(minX, v.x);
                minY = std::min(minY, v.y);
                maxX = std::max(maxX, v.x);
                maxY = std::max(maxY, v.y);
            }
        }
        if (seeded) {
            b = {minX, minY, maxX, maxY};
            bounds = b;
            hasBounds = true;
        }
    }
    if (!hasBounds) {
        return false;
    }

    Vec2 minPos{};
    Vec2 maxPos{};
    bool first = true;
    const std::size_t count = vertices.size();
    for (std::size_t i = 0; i < count; ++i) {
        Vec2 pos = vertices[i];
        if (i < deformation.size()) {
            auto d = deformation[i];
            pos.x += d.x;
            pos.y += d.y;
        }
        if (first) {
            minPos = pos;
            maxPos = pos;
            first = false;
        } else {
            minPos.x = std::min(minPos.x, pos.x);
            minPos.y = std::min(minPos.y, pos.y);
            maxPos.x = std::max(maxPos.x, pos.x);
            maxPos.y = std::max(maxPos.y, pos.y);
        }
    }

    Vec2 size{maxPos.x - minPos.x, maxPos.y - minPos.y};
    if (!std::isfinite(size.x) || !std::isfinite(size.y) || size.x <= 0 || size.y <= 0) {
        return false;
    }
    texWidth = static_cast<uint32_t>(std::ceil(size.x)) + 1;
    texHeight = static_cast<uint32_t>(std::ceil(size.y)) + 1;
    Vec2 deformOffset = deformationTranslationOffset();
    auto t = transform();
    textureOffset = Vec2{(b[0]+b[2])/2.0f, (b[1]+b[3])/2.0f};
    textureOffset.x += deformOffset.x - t.translation.x;
    textureOffset.y += deformOffset.y - t.translation.y;
    textures = {};
    textures[0] = std::make_shared<Texture>(static_cast<int>(texWidth), static_cast<int>(texHeight));
    stencil = std::make_shared<Texture>(static_cast<int>(texWidth), static_cast<int>(texHeight), 1, true);
    if (prevTexture) prevTexture->dispose();
    if (prevStencil) prevStencil->dispose();
    if (offscreenSurface) {
        if (auto backend = core::getCurrentRenderBackend()) {
            backend->destroyDynamicComposite(offscreenSurface);
            offscreenSurface->framebuffer = 0;
        }
    }
    reuseCachedTextureThisFrame = false;
    initialized = true;
    textureInvalidated = true;
    hasValidOffscreenContent = false;
    loggedFirstRenderAttempt = false;
    return true;
}

bool Projectable::updateDynamicRenderStateFlags() {
    bool resized = false;
    auto frameId = currentProjectableFrame();
    if (autoResizedMesh && detectAncestorTransformChange(frameId)) {
        deferredChanged = true;
        useMaxChildrenBounds = false;
    }
    if (deferredChanged) {
        if (autoResizedMesh) {
            bool ran = false;
            resized = updateAutoResizedMeshOnce(ran);
            if (ran && resized) {
                initialized = false;
            }
            if (ran) {
                deferredChanged = false;
                textureInvalidated = true;
                hasValidOffscreenContent = false;
                loggedFirstRenderAttempt = false;
            }
        } else {
            deferredChanged = false;
            textureInvalidated = true;
            hasValidOffscreenContent = false;
            loggedFirstRenderAttempt = false;
        }
    }
    if (autoResizedMesh && boundsDirty) {
        bool resizedNow = createSimpleMesh();
        boundsDirty = false;
        if (resizedNow) {
            textureInvalidated = true;
            initialized = false;
        }
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

void Projectable::dynamicRenderBegin(core::RenderContext& ctx) {
    dynamicScopeActive = false;
    dynamicScopeToken = static_cast<std::size_t>(-1);
    reuseCachedTextureThisFrame = false;
    if (!hasValidOffscreenContent) {
        textureInvalidated = true;
    }
    queuedOffscreenParts.clear();
    if (!renderEnabled() || !ctx.renderGraph) return;
    updateDynamicRenderStateFlags();
    bool needsRedraw = textureInvalidated || deferred > 0;
    if (!needsRedraw) {
        reuseCachedTextureThisFrame = true;
        loggedFirstRenderAttempt = true;
        return;
    }

    selfSort();
    auto pass = prepareDynamicCompositePass();
    if (!pass.surface) {
        reuseCachedTextureThisFrame = true;
        loggedFirstRenderAttempt = true;
        return;
    }
    dynamicScopeToken = ctx.renderGraph->pushDynamicComposite(std::dynamic_pointer_cast<Projectable>(shared_from_this()), pass, zSort());
    dynamicScopeActive = true;

    Mat4 translate = Mat4::translation(Vec3{-textureOffset.x, -textureOffset.y, 0});
    Mat4 correction = Mat4::multiply(fullTransformMatrix(), Mat4::inverse(transform().toMat4()));
    Mat4 childBasis = Mat4::multiply(translate, Mat4::inverse(transform().toMat4()));
    queuedOffscreenParts.clear();

    for (auto& p : subParts) {
        if (!p) continue;
        if (std::dynamic_pointer_cast<Mask>(p)) continue;
        Mat4 childMatrix = Mat4::multiply(correction, p->transform().toMat4());
        Mat4 finalMatrix = Mat4::multiply(childBasis, childMatrix);
        p->setOffscreenModelMatrix(finalMatrix);
        if (auto dynChild = std::dynamic_pointer_cast<Projectable>(p)) {
            dynChild->renderNestedOffscreen(ctx);
        } else {
            p->enqueueRenderCommands(ctx);
        }
        queuedOffscreenParts.push_back(p);
    }
    for (auto& m : maskParts) {
        if (!m) continue;
        Mat4 maskMatrix = Mat4::multiply(correction, m->transform().toMat4());
        Mat4 finalMatrix = Mat4::multiply(translate, maskMatrix);
        m->setOffscreenModelMatrix(finalMatrix);
        m->enqueueRenderCommands(ctx);
        queuedOffscreenParts.push_back(m);
    }
}

void Projectable::dynamicRenderEnd(core::RenderContext& ctx) {
    if (!ctx.renderGraph) return;
    auto pup = puppetRef();
    bool redrew = dynamicScopeActive;
    if (dynamicScopeActive) {
        auto queuedForCleanup = queuedOffscreenParts;

        auto dedupMaskBindings = [&]() {
            std::vector<MaskBinding> result;
            std::set<uint64_t> seen;
            for (auto m : masks) {
                auto maskPtr = m.maskSrc;
                if (!maskPtr && pup && m.maskSrcUUID != 0) {
                    auto node = pup->findNodeById(m.maskSrcUUID);
                    maskPtr = std::dynamic_pointer_cast<Drawable>(node);
                }
                if (!maskPtr) continue;
                m.maskSrc = maskPtr;
                uint64_t key = (static_cast<uint64_t>(maskPtr->uuid) << 32) | static_cast<uint32_t>(m.mode);
                if (seen.count(key)) continue;
                seen.insert(key);
                result.push_back(m);
            }
            return result;
        };
        auto maskBindings = dedupMaskBindings();
        bool useStencil = false;
        for (const auto& m : maskBindings) {
            if (m.mode == MaskingMode::Mask) { useStencil = true; break; }
        }
        auto self = std::dynamic_pointer_cast<Projectable>(shared_from_this());
        ctx.renderGraph->popDynamicComposite(dynamicScopeToken, [self, queuedForCleanup, maskBindings, useStencil](core::RenderCommandEmitter& emitter) {
            if (!self) return;
            if (!maskBindings.empty()) {
                emitter.beginMask(useStencil);
                for (auto binding : maskBindings) {
                    auto maskPtr = binding.maskSrc;
                    if (!maskPtr) {
                        if (auto pup = self->puppetRef()) {
                            auto node = pup->findNodeById(binding.maskSrcUUID);
                            maskPtr = std::dynamic_pointer_cast<Drawable>(node);
                        }
                    }
                    if (!maskPtr) continue;
                    bool isDodge = binding.mode == MaskingMode::DodgeMask;
                    emitter.applyMask(maskPtr, isDodge);
                }
                emitter.beginMaskContent();
            }

            emitter.drawPart(self, false);

            if (!maskBindings.empty()) {
                emitter.endMask();
            }

            for (auto part : queuedForCleanup) {
                if (auto p = std::dynamic_pointer_cast<Part>(part)) {
                    p->clearOffscreenModelMatrix();
                    p->clearOffscreenRenderMatrix();
                } else if (auto m = std::dynamic_pointer_cast<Mask>(part)) {
                    m->clearOffscreenModelMatrix();
                    m->clearOffscreenRenderMatrix();
                }
            }
        });
    } else {
        auto cleanupParts = queuedOffscreenParts;
        Part::enqueueRenderCommands(ctx, [cleanupParts](core::RenderCommandEmitter&) {
            for (auto& part : cleanupParts) {
                if (auto p = std::dynamic_pointer_cast<Part>(part)) {
                    p->clearOffscreenModelMatrix();
                    p->clearOffscreenRenderMatrix();
                } else if (auto m = std::dynamic_pointer_cast<Mask>(part)) {
                    m->clearOffscreenModelMatrix();
                    m->clearOffscreenRenderMatrix();
                }
            }
        });
    }

    reuseCachedTextureThisFrame = false;
    queuedOffscreenParts.clear();
    if (redrew) {
        textureInvalidated = false;
        if (deferred > 0) deferred--;
        hasValidOffscreenContent = true;
    }
    dynamicScopeActive = false;
    dynamicScopeToken = static_cast<std::size_t>(-1);
}

void Projectable::renderNestedOffscreen(core::RenderContext& ctx) {
    dynamicRenderBegin(ctx);
    dynamicRenderEnd(ctx);
}

void Projectable::registerRenderTasks(core::TaskScheduler& scheduler) {
    scheduler.addTask(core::TaskOrder::Init, core::TaskKind::Init, [this](core::RenderContext& ctx) { runBeginTask(ctx); });
    scheduler.addTask(core::TaskOrder::PreProcess, core::TaskKind::PreProcess, [this](core::RenderContext& ctx) { runPreProcessTask(ctx); });
    scheduler.addTask(core::TaskOrder::Dynamic, core::TaskKind::Dynamic, [this](core::RenderContext& ctx) { runDynamicTask(ctx); });
    scheduler.addTask(core::TaskOrder::Post0, core::TaskKind::PostProcess, [this](core::RenderContext& ctx) { runPostTaskImpl(0, ctx); });
    scheduler.addTask(core::TaskOrder::Post1, core::TaskKind::PostProcess, [this](core::RenderContext& ctx) { runPostTaskImpl(1, ctx); });
    scheduler.addTask(core::TaskOrder::Post2, core::TaskKind::PostProcess, [this](core::RenderContext& ctx) { runPostTaskImpl(2, ctx); });

    bool allowRenderTasks = !hasProjectableAncestor(std::dynamic_pointer_cast<Projectable>(shared_from_this()));
    if (allowRenderTasks) {
    scheduler.addTask(core::TaskOrder::RenderBegin, core::TaskKind::Render, [this](core::RenderContext& ctx) { runRenderBeginTask(ctx); });
    scheduler.addTask(core::TaskOrder::Render, core::TaskKind::Render, [this](core::RenderContext& ctx) { runRenderTask(ctx); });
    }

    scheduler.addTask(core::TaskOrder::Final, core::TaskKind::Finalize, [this](core::RenderContext& ctx) { runFinalTask(ctx); });

    auto orderedChildren = childrenRef();
    std::sort(orderedChildren.begin(), orderedChildren.end(), [](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
        if (!a || !b) return false;
        return a->zSort() > b->zSort();
    });
    for (auto& child : orderedChildren) {
        if (child) child->registerRenderTasks(scheduler);
    }
    if (allowRenderTasks) {
        scheduler.addTask(core::TaskOrder::RenderEnd, core::TaskKind::Render, [this](core::RenderContext& ctx) { runRenderEndTask(ctx); });
    }
}

void Projectable::registerDelegatedTasks(core::TaskScheduler& scheduler) {
    scheduler.addTask(core::TaskOrder::Init, core::TaskKind::Init, [this](core::RenderContext& ctx) { runBeginTask(ctx); });
    scheduler.addTask(core::TaskOrder::PreProcess, core::TaskKind::PreProcess, [this](core::RenderContext& ctx) { runPreProcessTask(ctx); });
    scheduler.addTask(core::TaskOrder::Dynamic, core::TaskKind::Dynamic, [this](core::RenderContext& ctx) { runDynamicTask(ctx); });
    scheduler.addTask(core::TaskOrder::Post0, core::TaskKind::PostProcess, [this](core::RenderContext& ctx) { runPostTaskImpl(0, ctx); });
    scheduler.addTask(core::TaskOrder::Post1, core::TaskKind::PostProcess, [this](core::RenderContext& ctx) { runPostTaskImpl(1, ctx); });
    scheduler.addTask(core::TaskOrder::Post2, core::TaskKind::PostProcess, [this](core::RenderContext& ctx) { runPostTaskImpl(2, ctx); });
    scheduler.addTask(core::TaskOrder::Final, core::TaskKind::Finalize, [this](core::RenderContext& ctx) { runFinalTask(ctx); });
}

void Projectable::delegatedRunRenderBeginTask(core::RenderContext& ctx) { dynamicRenderBegin(ctx); }
void Projectable::delegatedRunRenderTask(core::RenderContext&) {}
void Projectable::delegatedRunRenderEndTask(core::RenderContext& ctx) { dynamicRenderEnd(ctx); }

void Projectable::renderMask(bool dodge) {
    Part::renderMask(dodge);
}

void Projectable::enqueueRenderCommands(core::RenderContext& ctx) {
    Part::enqueueRenderCommands(ctx);
}

void Projectable::runRenderTask(core::RenderContext&) {}

void Projectable::runDynamicTask(core::RenderContext& ctx) {
    if (autoResizedMesh) {
        if (shouldUpdateVertices) {
            shouldUpdateVertices = false;
        }
        bool ran = false;
        if (updateAutoResizedMeshOnce(ran) && ran) {
            initialized = false;
        }
    } else {
        Part::runDynamicTask(ctx);
    }
}

void Projectable::runRenderBeginTask(core::RenderContext& ctx) {
    dynamicRenderBegin(ctx);
}

void Projectable::runRenderEndTask(core::RenderContext& ctx) {
    dynamicRenderEnd(ctx);
}

void Projectable::runBeginTask(core::RenderContext& ctx) {
    Part::runBeginTask(ctx);
}

void Projectable::runPostTaskImpl(std::size_t priority, core::RenderContext& ctx) {
    Part::runPostTaskImpl(priority, ctx);
}

void Projectable::notifyChange(const std::shared_ptr<Node>& target, NotifyReason reason) {
    if (target != shared_from_this()) {
        if (reason == NotifyReason::AttributeChanged) {
            scanSubParts(childrenRef());
        }
        if (reason != NotifyReason::Initialized) {
            textureInvalidated = true;
            hasValidOffscreenContent = false;
            loggedFirstRenderAttempt = false;
        }
        if (reason == NotifyReason::Transformed) {
            enableMaxChildrenBounds(target);
        } else {
            invalidateChildrenBounds();
        }
        boundsDirty = true;
    } else if (reason == NotifyReason::AttributeChanged) {
        textureInvalidated = true;
        hasValidOffscreenContent = false;
        loggedFirstRenderAttempt = false;
        invalidateChildrenBounds();
        boundsDirty = true;
    } else if (reason == NotifyReason::Transformed) {
        textureInvalidated = true;
        hasValidOffscreenContent = false;
        loggedFirstRenderAttempt = false;
        invalidateChildrenBounds();
        boundsDirty = true;
    }

    if (autoResizedMesh && reason == NotifyReason::AttributeChanged) {
        bool ran = false;
        if (updateAutoResizedMeshOnce(ran) && ran) {
            initialized = false;
        }
    }

    Part::notifyChange(target, reason);
}

void Projectable::rebuffer(const MeshData& data) {
    autoResizedMesh = data.vertices.empty();
    Part::rebuffer(data);
    initialized = false;
    setIgnorePuppet(false);
    notifyChange(shared_from_this(), NotifyReason::Transformed);
}

bool Projectable::setupChild(const std::shared_ptr<Node>& child) {
    setIgnorePuppetRecurse(child, true);
    if (auto pup = puppetRef()) pup->rescanNodes();
    forceResize = true;
    invalidateChildrenBounds();
    boundsDirty = true;
    return false;
}

bool Projectable::releaseChild(const std::shared_ptr<Node>& child) {
    setIgnorePuppetRecurse(child, false);
    scanSubParts(childrenRef());
    forceResize = true;
    invalidateChildrenBounds();
    boundsDirty = true;
    return false;
}

void Projectable::setupSelf() {
    transformChanged();
    scanSubParts(childrenRef());
    if (autoResizedMesh) {
        if (createSimpleMesh()) initialized = false;
    }
    textureInvalidated = true;
    boundsDirty = false;
    hasCachedAncestorTransform = false;
    lastAncestorTransformCheckFrame = static_cast<std::size_t>(-1);
}

void Projectable::releaseSelf() {
    hasCachedAncestorTransform = false;
    lastAncestorTransformCheckFrame = static_cast<std::size_t>(-1);
}

void Projectable::onAncestorChanged(const std::shared_ptr<Node>&, NotifyReason reason) {
    if (!autoResizedMesh) return;
    if (reason == NotifyReason::Transformed) {
        auto frameId = currentProjectableFrame();
        if (pendingAncestorChangeFrame != frameId) {
            pendingAncestorChangeFrame = frameId;
            deferredChanged = true;
            ancestorChangeQueued = true;
            useMaxChildrenBounds = false;
        }
    }
}

void Projectable::serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const {
    auto savedTextures = textures;
    auto savedStencil = stencil;
    const_cast<Projectable*>(this)->textures = {nullptr, nullptr, nullptr};
    const_cast<Projectable*>(this)->stencil.reset();
    Part::serializeSelfImpl(serializer, recursive, flags);
    if (has_flag(flags, SerializeNodeFlags::State)) {
        serializer.putKey("auto_resized");
        serializer.putValue(autoResizedMesh);
        serializer.putKey("texture_offset_x");
        serializer.putValue(textureOffset.x);
        serializer.putKey("texture_offset_y");
        serializer.putValue(textureOffset.y);
        serializer.putKey("use_max_children_bounds");
        serializer.putValue(useMaxChildrenBounds);
        serializer.putKey("max_bounds_start_frame");
        serializer.putValue(maxBoundsStartFrame);
        serializer.putKey("deferred_changed");
        serializer.putValue(deferredChanged);
        serializer.putKey("ancestor_change_queued");
        serializer.putValue(ancestorChangeQueued);
    }
    const_cast<Projectable*>(this)->textures = savedTextures;
    const_cast<Projectable*>(this)->stencil = savedStencil;
}

::nicxlive::core::serde::SerdeException Projectable::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    auto res = Part::deserializeFromFghj(data);
    textures = {};
    if (auto ar = data.get_optional<bool>("auto_resized")) {
        autoResizedMesh = *ar;
    } else if (!mesh->indices.empty()) {
        autoResizedMesh = false;
    } else {
        autoResizedMesh = true;
    }
    if (auto tx = data.get_optional<float>("texture_offset_x")) textureOffset.x = *tx;
    if (auto ty = data.get_optional<float>("texture_offset_y")) textureOffset.y = *ty;
    if (auto ub = data.get_optional<bool>("use_max_children_bounds")) useMaxChildrenBounds = *ub;
    if (auto mb = data.get_optional<uint64_t>("max_bounds_start_frame")) maxBoundsStartFrame = *mb;
    if (auto dc = data.get_optional<bool>("deferred_changed")) deferredChanged = *dc;
    if (auto ac = data.get_optional<bool>("ancestor_change_queued")) ancestorChangeQueued = *ac;
    return res;
}

} // namespace nicxlive::core::nodes
