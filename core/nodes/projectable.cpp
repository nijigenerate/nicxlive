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

static void scanSubParts(const std::shared_ptr<Projectable>& self) {
    if (!self) return;
    self->subParts.clear();
    self->maskParts.clear();
    self->scanPartsRecurse(self);
    self->invalidateChildrenBounds();
    self->boundsDirty = true;
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

void Projectable::scanPartsRecurse(const std::shared_ptr<Node>& node) {
    if (!node) return;
    auto proj = std::dynamic_pointer_cast<Projectable>(node);
    auto part = std::dynamic_pointer_cast<Part>(node);
    auto mask = std::dynamic_pointer_cast<Mask>(node);
    if (part && node.get() != this) {
        subParts.push_back(part);
        if (!proj) {
            for (auto& c : part->childrenRef()) scanPartsRecurse(c);
        } else {
            proj->scanPartsRecurse(proj);
        }
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
    auto self = const_cast<Projectable*>(this);
    self->localTransform.update();
    self->offsetTransform.update();
    Transform base = self->localTransform.calcOffset(self->offsetTransform);
    if (lockToRootValue()) {
        if (auto pup = puppetRef()) {
            if (auto root = pup->root) {
                return base.calcOffset(root->localTransform);
            } else {
                return base.calcOffset(pup->transform);
            }
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
    maxBoundsStartFrame = currentProjectableFrame();
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
        rebuffer(data);
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
            mesh.vertices = vertexArray;
            mesh.indices = {0, 1, 2, 2, 1, 3};
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
    auto prevTexture = textures[0];
    auto prevStencil = stencil;
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
    auto frameId = currentProjectableFrame();
    if (deferredChanged) {
        if (autoResizedMesh) {
            bool ran = false;
            bool resized = updateAutoResizedMeshOnce(ran);
            pendingAncestorChangeFrame = static_cast<std::size_t>(-1);
            if (ancestorChangeQueued) {
                auto full = fullTransform();
                prevTranslation = full.translation;
                prevRotation = full.rotation;
                prevScale = Vec2{full.scale.x, full.scale.y};
                ancestorChangeQueued = false;
            }
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
    if (autoResizedMesh && pendingAncestorChangeFrame == frameId) {
        auto full = fullTransform();
        if (full.translation.x != prevTranslation.x || full.translation.y != prevTranslation.y ||
            full.rotation.z != prevRotation.z || full.scale.x != prevScale.x || full.scale.y != prevScale.y) {
            textureInvalidated = true;
            initialized = false;
        }
        prevTranslation = full.translation;
        prevRotation = full.rotation;
        prevScale = Vec2{full.scale.x, full.scale.y};
        pendingAncestorChangeFrame = static_cast<std::size_t>(-1);
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
    reuseCachedTextureThisFrame = false;
    hasValidOffscreenContent = false;
    auto pass = prepareDynamicCompositePass();
    if (ctx.renderBackend && pass.surface) {
        ctx.renderBackend->beginDynamicComposite(pass);
    }
}

void Projectable::dynamicRenderEnd(core::RenderContext& ctx) {
    auto pass = prepareDynamicCompositePass();
    if (ctx.renderBackend && pass.surface) {
        ctx.renderBackend->endDynamicComposite(pass);
    }
    hasValidOffscreenContent = true;
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
    std::stable_sort(orderedChildren.begin(), orderedChildren.end(), [](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
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
    selfSort();
    if (!ctx.renderGraph) return;

    auto passData = prepareDynamicCompositePass();
    dynamicScopeToken = ctx.renderGraph->pushDynamicComposite(std::dynamic_pointer_cast<Projectable>(shared_from_this()), passData, zSort());
    dynamicScopeActive = true;

    Mat4 translate = Mat4::translation(Vec3{-textureOffset.x, -textureOffset.y, 0});
    Mat4 correction = Mat4::multiply(fullTransformMatrix(), Mat4::inverse(transform().toMat4()));
    Mat4 childBasis = Mat4::multiply(translate, Mat4::inverse(transform().toMat4()));
    queuedOffscreenParts.clear();

    for (auto& p : subParts) {
        if (!p) continue;
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

void Projectable::runRenderTask(core::RenderContext& ctx) {
    if (!updateDynamicRenderStateFlags()) return;
    if (!ctx.renderGraph) {
        dynamicRenderBegin(ctx);
        dynamicRenderEnd(ctx);
        return;
    }
    enqueueRenderCommands(ctx);
    if (dynamicScopeActive) {
        auto queuedForCleanup = queuedOffscreenParts;

        auto dedupMaskBindings = [&]() {
            std::vector<MaskBinding> result;
            std::set<uint64_t> seen;
            for (const auto& m : masks) {
                if (!m.maskSrc) continue;
                uint64_t key = (static_cast<uint64_t>(m.maskSrcUUID) << 32) | static_cast<uint32_t>(m.mode);
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
                    if (!binding.maskSrc) continue;
                    bool isDodge = binding.mode == MaskingMode::DodgeMask;
                    emitter.applyMask(binding.maskSrc, isDodge);
                }
                emitter.beginMaskContent();
            }

            emitter.drawPart(self, false);

            if (!maskBindings.empty()) {
                emitter.endMask();
            }

            for (auto part : queuedForCleanup) {
                if (auto p = std::dynamic_pointer_cast<Part>(part)) p->clearOffscreenModelMatrix();
                else if (auto m = std::dynamic_pointer_cast<Mask>(part)) m->clearOffscreenModelMatrix();
            }
        });
    }
    reuseCachedTextureThisFrame = false;
    queuedOffscreenParts.clear();
    if (dynamicScopeActive) {
        textureInvalidated = false;
        if (deferred > 0) deferred--;
        hasValidOffscreenContent = true;
    }
    dynamicScopeActive = false;
    dynamicScopeToken = static_cast<std::size_t>(-1);
}

void Projectable::runRenderBeginTask(core::RenderContext& ctx) {
    dynamicRenderBegin(ctx);
}

void Projectable::runRenderEndTask(core::RenderContext& ctx) {
    dynamicRenderEnd(ctx);
}

void Projectable::runBeginTask(core::RenderContext& ctx) {
    Part::runBeginTask(ctx);
    useMaxChildrenBounds = false;
    ancestorChangeQueued = false;
    pendingAncestorChangeFrame = static_cast<std::size_t>(-1);
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
    if (target != shared_from_this()) {
        if (reason == NotifyReason::AttributeChanged) {
            scanPartsRecurse(shared_from_this());
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

    if (autoResizedMesh && reason == NotifyReason::Transformed) {
        auto frameId = currentProjectableFrame();
        if (pendingAncestorChangeFrame != frameId) {
            pendingAncestorChangeFrame = frameId;
            deferredChanged = true;
            ancestorChangeQueued = true;
            useMaxChildrenBounds = false;
        }
    }

    if (autoResizedMesh && reason == NotifyReason::AttributeChanged) {
        bool ran = false;
        if (updateAutoResizedMeshOnce(ran) && ran) {
            initialized = false;
        }
    }

    Part::notifyChange(target, reason);
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
    scanSubParts(std::dynamic_pointer_cast<Projectable>(shared_from_this()));
    forceResize = true;
    invalidateChildrenBounds();
    boundsDirty = true;
    return false;
}

void Projectable::setupSelf() {
    transformChanged();
    scanSubParts(std::dynamic_pointer_cast<Projectable>(shared_from_this()));
    if (autoResizedMesh) {
        if (createSimpleMesh()) initialized = false;
    }
    textureInvalidated = true;
    boundsDirty = false;
    auto selfNode = std::dynamic_pointer_cast<Node>(shared_from_this());
    for (auto c = shared_from_this(); c; c = c->parentPtr()) {
        c->addNotifyListener(selfNode);
    }
}

void Projectable::releaseSelf() {
    auto selfNode = std::dynamic_pointer_cast<Node>(shared_from_this());
    for (auto c = shared_from_this(); c; c = c->parentPtr()) {
        c->removeNotifyListener(selfNode);
    }
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
    return res;
}

} // namespace nicxlive::core::nodes
