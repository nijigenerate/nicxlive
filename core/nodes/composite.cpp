#include "composite.hpp"
#include "../runtime_state.hpp"
#include "../render/common.hpp"
#include "../puppet.hpp"
#include <sstream>
#include <cmath>

namespace nicxlive::core::nodes {

constexpr uint64_t MaxBoundsResetInterval = 120;

static bool hasProjectableAncestorNode(const Composite* self) {
    if (!self) return false;
    auto node = self->parentRef();
    while (node) {
        if (std::dynamic_pointer_cast<Projectable>(node)) return true;
        node = node->parentRef();
    }
    return false;
}

Composite::Composite(const MeshData& data, uint32_t uuidVal) : Projectable() {
    *mesh = data;
    if (uuidVal != 0) uuid = uuidVal;
    autoResizedMesh = true;
}

const std::string& Composite::typeId() const {
    static const std::string k = "Composite";
    return k;
}

bool Composite::propagateMeshGroupEnabled() const { return propagateMeshGroup; }
void Composite::setPropagateMeshGroup(bool value) { propagateMeshGroup = value; }

float Composite::threshold() const { return maskAlphaThreshold; }
void Composite::setThreshold(float value) { maskAlphaThreshold = value; }

Transform Composite::transform() { return Part::transform(); }
Transform Composite::transform() const { return Part::transform(); }

bool Composite::mustPropagate() const { return propagateMeshGroup; }

void Composite::serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const {
    // Texturesを一時退避して状態のみをシリアライズ（D相当）
    auto savedTextures = textures;
    auto savedStencil = stencil;
    const_cast<Composite*>(this)->textures = {nullptr, nullptr, nullptr};
    const_cast<Composite*>(this)->stencil.reset();
    Node::serializeSelfImpl(serializer, recursive, flags);
    if (has_flag(flags, SerializeNodeFlags::State)) {
        serializer.putKey("blend_mode");
        serializer.putValue(static_cast<int>(blendMode));
        serializer.putKey("tint");
        serializer.putValue(std::string{std::to_string(tint.x) + "," + std::to_string(tint.y) + "," + std::to_string(tint.z)});
        serializer.putKey("screenTint");
        serializer.putValue(std::string{std::to_string(screenTint.x) + "," + std::to_string(screenTint.y) + "," + std::to_string(screenTint.z)});
        serializer.putKey("mask_threshold");
        serializer.putValue(maskAlphaThreshold);
        serializer.putKey("opacity");
        serializer.putValue(opacity);
        serializer.putKey("propagate_meshgroup");
        serializer.putValue(propagateMeshGroup);
    }
    if (has_flag(flags, SerializeNodeFlags::Links) && !masks.empty()) {
        serializer.putKey("masks");
        auto state = serializer.listBegin();
        for (const auto& m : masks) {
            serializer.elemBegin();
            serializer.putKey("source");
            serializer.putValue(static_cast<std::size_t>(m.maskSrcUUID));
            serializer.putKey("mode");
            serializer.putValue(static_cast<int>(m.mode));
        }
        serializer.listEnd(state);
    }
    const_cast<Composite*>(this)->textures = savedTextures;
    const_cast<Composite*>(this)->stencil = savedStencil;
}

::nicxlive::core::serde::SerdeException Composite::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    auto result = Node::deserializeFromFghj(data);
    if (auto op = data.get_optional<float>("opacity")) opacity = *op;
    if (auto thr = data.get_optional<float>("mask_threshold")) maskAlphaThreshold = *thr;
    if (auto tinStr = data.get_optional<std::string>("tint")) {
        char comma;
        Vec3 parsed = tint;
        std::stringstream ss(*tinStr);
        if (ss >> parsed.x >> comma >> parsed.y >> comma >> parsed.z) {
            tint = parsed;
        }
    } else if (auto tin = data.get_child_optional("tint")) {
        tint.x = tin->get<float>("x", tint.x);
        tint.y = tin->get<float>("y", tint.y);
        tint.z = tin->get<float>("z", tint.z);
    }
    if (auto stStr = data.get_optional<std::string>("screenTint")) {
        char comma;
        Vec3 parsed = screenTint;
        std::stringstream ss(*stStr);
        if (ss >> parsed.x >> comma >> parsed.y >> comma >> parsed.z) {
            screenTint = parsed;
        }
    } else if (auto st = data.get_child_optional("screenTint")) {
        screenTint.x = st->get<float>("x", screenTint.x);
        screenTint.y = st->get<float>("y", screenTint.y);
        screenTint.z = st->get<float>("z", screenTint.z);
    }
    if (auto bm = data.get_optional<int>("blend_mode")) {
        if (*bm >= 0 && *bm <= static_cast<int>(BlendMode::SliceFromLower)) {
            blendMode = static_cast<BlendMode>(*bm);
        }
    } else if (auto bs = data.get_optional<std::string>("blend_mode")) {
        if (auto parsed = parseBlendMode(*bs)) {
            blendMode = *parsed;
        }
    }
    if (auto pmg = data.get_optional<bool>("propagate_meshgroup")) {
        propagateMeshGroup = *pmg;
    } else {
        propagateMeshGroup = false;
    }
    masks.clear();
    if (auto ml = data.get_child_optional("masks")) {
        for (const auto& e : *ml) {
            MaskBinding mb;
            mb.maskSrcUUID = e.second.get<uint32_t>("source", 0);
            int modeVal = e.second.get<int>("mode", 0);
            if (modeVal >= 0 && modeVal <= static_cast<int>(MaskingMode::DodgeMask)) {
                mb.mode = static_cast<MaskingMode>(modeVal);
            }
            masks.push_back(mb);
        }
    }
    autoResizedMesh = true;
    textures = {};
    return result;
}

Mat4 Composite::childOffscreenMatrix(const std::shared_ptr<Part>& child) const {
    if (!child) return Mat4::identity();
    auto scale = compositeAutoScale();
    Mat4 scaleMat = Mat4::scale(Vec3{scale.x, scale.y, 1.0f});
    Vec2 offset = textureOffset;
    if (!std::isfinite(offset.x) || !std::isfinite(offset.y)) offset = Vec2{0, 0};
    auto trans = transform();
    Mat4 invTranslate = Mat4::translation(Vec3{-trans.translation.x * scale.x, -trans.translation.y * scale.y, 0.0f});
    Mat4 childLocal = Mat4::multiply(invTranslate, Mat4::multiply(scaleMat, child->transform().toMat4()));
    Mat4 translate = Mat4::translation(Vec3{-offset.x, -offset.y, 0});
    return Mat4::multiply(translate, childLocal);
}

Mat4 Composite::offscreenRenderMatrix() const {
    if (textures.empty() || !textures[0]) return Mat4::identity();
    float halfW = static_cast<float>(textures[0]->width()) / 2.0f;
    float halfH = static_cast<float>(textures[0]->height()) / 2.0f;
    auto onscreenMatrix = currentRenderSpace().matrix;
    float onscreenY = onscreenMatrix[1][1];
    float zHalf = static_cast<float>(std::numeric_limits<uint16_t>::max()) / 2.0f;
    auto ortho = Mat4::orthographic(-halfW, halfW, halfH, -halfH, -zHalf, zHalf);
    if (onscreenY != 0.0f && ((ortho[1][1] > 0.0f) != (onscreenY > 0.0f))) {
        ortho = Mat4::orthographic(-halfW, halfW, -halfH, halfH, -zHalf, zHalf);
    }
    auto flipY = Mat4::scale(Vec3{1.0f, -1.0f, 1.0f});
    return Mat4::multiply(flipY, ortho);
}

Mat4 Composite::childCoreMatrix(const std::shared_ptr<Part>& child) const {
    if (!child) return Mat4::identity();
    return child->transform().toMat4();
}

Vec4 Composite::localBoundsFromMatrix(const std::shared_ptr<Part>& child, const Mat4& matrix) const {
    float tx = matrix[0][3];
    float ty = matrix[1][3];
    Vec4 bounds{tx, ty, tx, ty};
    if (!child || child->vertices.size() == 0) return bounds;
    auto deform = child->deformation;
    for (std::size_t i = 0; i < child->vertices.size(); ++i) {
        Vec2 localVertex = child->vertices[i];
        if (i < deform.size()) {
            auto d = deform[i];
            localVertex.x += d.x;
            localVertex.y += d.y;
        }
        auto vertOriented = matrix.transformPoint(Vec3{localVertex.x, localVertex.y, 0});
        bounds.x = std::min(bounds.x, vertOriented.x);
        bounds.y = std::min(bounds.y, vertOriented.y);
        bounds.z = std::max(bounds.z, vertOriented.x);
        bounds.w = std::max(bounds.w, vertOriented.y);
    }
    return bounds;
}

Vec4 Composite::getChildrenBounds(bool forceUpdate) {
    auto scale = compositeAutoScale();
    auto frameId = currentProjectableFrame();
    if (useMaxChildrenBounds) {
        if (frameId - maxBoundsStartFrame < MaxBoundsResetInterval) {
            if (hasMaxChildrenBoundsLocal) {
                Vec4 scaled = maxChildrenBoundsLocal;
                scaled.x *= scale.x;
                scaled.z *= scale.x;
                scaled.y *= scale.y;
                scaled.w *= scale.y;
                return scaled;
            }
            return maxChildrenBounds;
        }
        useMaxChildrenBounds = false;
        hasMaxChildrenBoundsLocal = false;
    }

    if (forceUpdate) {
        for (auto& p : subParts) {
            if (p) p->updateBounds();
        }
    }

    Vec4 bounds{0, 0, 0, 0};
    Vec4 localBounds{0, 0, 0, 0};
    bool useMatrixBounds = autoResizedMesh;
    if (useMatrixBounds) {
        bool hasBounds = false;
        for (auto& part : subParts) {
            if (!part) continue;
            auto childMatrix = childCoreMatrix(part);
            auto childBounds = localBoundsFromMatrix(part, childMatrix);
            if (!hasBounds) {
                localBounds = childBounds;
                hasBounds = true;
            } else {
                localBounds.x = std::min(localBounds.x, childBounds.x);
                localBounds.y = std::min(localBounds.y, childBounds.y);
                localBounds.z = std::max(localBounds.z, childBounds.z);
                localBounds.w = std::max(localBounds.w, childBounds.w);
            }
        }
        if (!hasBounds) {
            auto t = transform();
            localBounds = Vec4{t.translation.x, t.translation.y, t.translation.x, t.translation.y};
        }
        bounds = localBounds;
        bounds.x *= scale.x;
        bounds.z *= scale.x;
        bounds.y *= scale.y;
        bounds.w *= scale.y;
    } else {
        std::vector<Vec4> childBoundsList;
        childBoundsList.reserve(subParts.size());
        for (auto& p : subParts) {
            if (!p || !p->bounds) continue;
            childBoundsList.push_back(Vec4{(*p->bounds)[0], (*p->bounds)[1], (*p->bounds)[2], (*p->bounds)[3]});
        }
        auto t = transform();
        bounds = mergeBounds(childBoundsList, Vec4{t.translation.x, t.translation.y, t.translation.x, t.translation.y});
    }

    if (!useMaxChildrenBounds) {
        maxChildrenBounds = bounds;
        useMaxChildrenBounds = true;
        maxBoundsStartFrame = frameId;
        if (useMatrixBounds) {
            maxChildrenBoundsLocal = localBounds;
            hasMaxChildrenBoundsLocal = true;
        } else {
            hasMaxChildrenBoundsLocal = false;
        }
    }
    return bounds;
}

void Composite::enableMaxChildrenBounds(const std::shared_ptr<Node>& target) {
    auto targetDrawable = std::dynamic_pointer_cast<Drawable>(target);
    if (targetDrawable) {
        targetDrawable->updateBounds();
    }

    auto frameId = currentProjectableFrame();
    maxChildrenBounds = getChildrenBounds(false);
    useMaxChildrenBounds = true;
    maxBoundsStartFrame = frameId;

    if (autoResizedMesh) {
        auto scale = compositeAutoScale();
        maxChildrenBoundsLocal = maxChildrenBounds;
        if (scale.x != 0 && scale.y != 0) {
            maxChildrenBoundsLocal.x /= scale.x;
            maxChildrenBoundsLocal.z /= scale.x;
            maxChildrenBoundsLocal.y /= scale.y;
            maxChildrenBoundsLocal.w /= scale.y;
        }
        hasMaxChildrenBoundsLocal = true;
    } else {
        hasMaxChildrenBoundsLocal = false;
    }

    if (targetDrawable) {
        Vec4 b{};
        bool finite = false;
        if (targetDrawable->bounds) {
            b = Vec4{(*targetDrawable->bounds)[0], (*targetDrawable->bounds)[1], (*targetDrawable->bounds)[2], (*targetDrawable->bounds)[3]};
            finite = std::isfinite(b.x) && std::isfinite(b.y) && std::isfinite(b.z) && std::isfinite(b.w);
        }
        if (!finite) {
            if (auto targetPart = std::dynamic_pointer_cast<Part>(targetDrawable)) {
                b = localBoundsFromMatrix(targetPart, childCoreMatrix(targetPart));
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

bool Composite::createSimpleMesh() {
    auto bounds = getChildrenBounds(!useMaxChildrenBounds);
    Vec2 size{bounds.z - bounds.x, bounds.w - bounds.y};
    if (size.x <= 0 || size.y <= 0) return false;

    auto scale = compositeAutoScale();
    auto deformOffset = deformationTranslationOffsetLocal();
    Vec2 scaledDeformOffset{deformOffset.x * scale.x, deformOffset.y * scale.y};
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

    auto t = transform();
    Vec2 originOffset{t.translation.x * scale.x, t.translation.y * scale.y};
    originOffset.x += scaledDeformOffset.x;
    originOffset.y += scaledDeformOffset.y;
    auto makeVert = [&](float x, float y) {
        return Vec2{x + scaledDeformOffset.x - originOffset.x, y + scaledDeformOffset.y - originOffset.y};
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
        textureOffset = Vec2{(bounds.x + bounds.z) / 2.0f + scaledDeformOffset.x - originOffset.x,
                             (bounds.y + bounds.w) / 2.0f + scaledDeformOffset.y - originOffset.y};
    } else {
        Vec2 newTextureOffset{(bounds.x + bounds.z) / 2.0f + scaledDeformOffset.x - originOffset.x,
                              (bounds.y + bounds.w) / 2.0f + scaledDeformOffset.y - originOffset.y};
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

bool Composite::updateDynamicRenderStateFlags() {
    bool wasAuto = autoResizedMesh;

    auto currentScale = compositeAutoScale();
    auto currentRot = compositeRotation();
    auto camRot = cameraRotation();
    constexpr float scaleEps = 0.0001f;
    constexpr float rotEps = 0.0001f;
    bool scaleChanged = !hasPrevCompositeScale ||
        std::abs(currentScale.x - prevCompositeScale.x) > scaleEps ||
        std::abs(currentScale.y - prevCompositeScale.y) > scaleEps;
    bool rotChanged = !hasPrevCompositeScale ||
        std::abs(currentRot - prevCompositeRotation) > rotEps ||
        std::abs(camRot - prevCameraRotation) > rotEps;
    bool changed = false;
    if (scaleChanged || rotChanged) {
        prevCompositeScale = currentScale;
        prevCompositeRotation = currentRot;
        prevCameraRotation = camRot;
        hasPrevCompositeScale = true;
        useMaxChildrenBounds = false;
        changed = true;
    }

    bool base = Projectable::updateDynamicRenderStateFlags();
    return changed || (autoResizedMesh && !wasAuto) || base;
}

::nicxlive::core::DynamicCompositePass Composite::prepareDynamicCompositePass() {
    auto pass = Projectable::prepareDynamicCompositePass();
    pass.autoScaled = true;
    return pass;
}

void Composite::dynamicRenderBegin(core::RenderContext& ctx) {
    dynamicScopeActive = false;
    dynamicScopeToken = static_cast<std::size_t>(-1);
    reuseCachedTextureThisFrame = false;
    updateDynamicRenderStateFlags();

    if (!hasValidOffscreenContent) {
        textureInvalidated = true;
    }
    if (autoResizedMesh && createSimpleMesh()) {
        textureInvalidated = true;
    }
    queuedOffscreenParts.clear();

    if (textures.empty() || !textures[0]) {
        initialized = false;
        if (!initTarget()) {
            reuseCachedTextureThisFrame = true;
            loggedFirstRenderAttempt = true;
            return;
        }
    }
    if (!renderEnabled() || !ctx.renderGraph) {
        return;
    }

    bool needsRedraw = textureInvalidated || deferred > 0;
    if (!needsRedraw) {
        reuseCachedTextureThisFrame = true;
        loggedFirstRenderAttempt = true;
        return;
    }

    selfSort();
    auto passData = prepareDynamicCompositePass();
    if (!passData.surface) {
        reuseCachedTextureThisFrame = true;
        loggedFirstRenderAttempt = true;
        return;
    }

    dynamicScopeToken = ctx.renderGraph->pushDynamicComposite(
        std::dynamic_pointer_cast<Projectable>(shared_from_this()),
        passData,
        zSort());
    dynamicScopeActive = true;

    bool applyScreenSpace = !hasProjectableAncestorNode(this);
    Mat4 renderMatrix = applyScreenSpace ? offscreenRenderMatrix() : Mat4::identity();
    for (auto& child : subParts) {
        if (!child) continue;
        auto finalMatrix = childOffscreenMatrix(child);
        child->setOffscreenModelMatrix(finalMatrix);
        if (applyScreenSpace) {
            child->setOffscreenRenderMatrix(renderMatrix);
        } else {
            child->clearOffscreenRenderMatrix();
        }
        if (auto dynChild = std::dynamic_pointer_cast<Projectable>(child)) {
            dynChild->renderNestedOffscreen(ctx);
        } else {
            child->enqueueRenderCommands(ctx);
        }
        queuedOffscreenParts.push_back(child);
    }
}

void Composite::fillDrawPacket(const Node& header, PartDrawPacket& packet, bool isMask) const {
    Part::fillDrawPacket(header, packet, isMask);
    if (!packet.renderable) return;

    bool nested = hasProjectableAncestorNode(this);
    if (nested) {
        Mat4 m = packet.modelMatrix;
        auto localScale = transform().scale;
        float invLocalX = (localScale.x == 0.0f || !std::isfinite(localScale.x)) ? 1.0f : (1.0f / localScale.x);
        float invLocalY = (localScale.y == 0.0f || !std::isfinite(localScale.y)) ? 1.0f : (1.0f / localScale.y);
        auto invLocal = Mat4::scale(Vec3{invLocalX, invLocalY, 1.0f});
        m = Mat4::multiply(m, invLocal);

        float sx = std::sqrt(m[0][0] * m[0][0] + m[1][0] * m[1][0]);
        float sy = std::sqrt(m[0][1] * m[0][1] + m[1][1] * m[1][1]);
        if (sx == 0.0f) sx = 1.0f;
        if (sy == 0.0f) sy = 1.0f;
        m[0][0] = sx; m[0][1] = 0.0f;
        m[1][0] = 0.0f; m[1][1] = sy;
        packet.modelMatrix = m;
        return;
    }

    auto screen = transform();
    screen.rotation = Vec3{0.0f, 0.0f, 0.0f};
    screen.scale = Vec2{1.0f, 1.0f};
    screen.update();
    packet.modelMatrix = screen.toMat4();

    auto renderSpace = currentRenderSpace();
    auto renderMatrix = packet.renderMatrix;
    auto composed = Mat4::multiply(renderMatrix, packet.modelMatrix);
    auto origin = composed.transformPoint(Vec3{0.0f, 0.0f, 0.0f});
    float invScaleX = (renderSpace.scale.x == 0.0f) ? 1.0f : (1.0f / renderSpace.scale.x);
    float invScaleY = (renderSpace.scale.y == 0.0f) ? 1.0f : (1.0f / renderSpace.scale.y);
    if (!std::isfinite(invScaleX)) invScaleX = 1.0f;
    if (!std::isfinite(invScaleY)) invScaleY = 1.0f;
    float rot = renderSpace.rotation;
    if (!std::isfinite(rot)) rot = 0.0f;
    auto cancel = Mat4::multiply(
        Mat4::multiply(
            Mat4::multiply(
                Mat4::translation(Vec3{origin.x, origin.y, 0.0f}),
                Mat4::zRotation(-rot)),
            Mat4::scale(Vec3{invScaleX, invScaleY, 1.0f})),
        Mat4::translation(Vec3{-origin.x, -origin.y, 0.0f}));
    packet.renderMatrix = Mat4::multiply(cancel, renderMatrix);
    packet.renderRotation = 0.0f;
}

Vec2 Composite::deformationTranslationOffsetLocal() const {
    if (deformation.size() == 0) return Vec2{0, 0};
    Vec2 base = deformation[0];
    constexpr float eps = 0.0001f;
    for (std::size_t i = 0; i < deformation.size(); ++i) {
        auto d = deformation[i];
        if (std::abs(d.x - base.x) > eps || std::abs(d.y - base.y) > eps) {
            return Vec2{0, 0};
        }
    }
    return base;
}

Vec2 Composite::compositeAutoScale() const {
    if (hasProjectableAncestorNode(this)) {
        return Vec2{1, 1};
    }
    const auto& camera = ::nicxlive::core::inGetCamera();
    Vec2 scale = camera.scale;
    if (!std::isfinite(scale.x) || scale.x == 0) scale.x = 1;
    if (!std::isfinite(scale.y) || scale.y == 0) scale.y = 1;
    if (auto pup = puppetRef()) {
        auto puppetScale = pup->transform.scale;
        if (!std::isfinite(puppetScale.x) || puppetScale.x == 0) puppetScale.x = 1;
        if (!std::isfinite(puppetScale.y) || puppetScale.y == 0) puppetScale.y = 1;
        scale.x *= puppetScale.x;
        scale.y *= puppetScale.y;
    }
    return Vec2{std::abs(scale.x), std::abs(scale.y)};
}

float Composite::compositeRotation() const {
    float rot = transform().rotation.z;
    if (!std::isfinite(rot)) rot = 0;
    return rot;
}

float Composite::cameraRotation() const {
    const auto& camera = ::nicxlive::core::inGetCamera();
    float rot = camera.rotation;
    if (!std::isfinite(rot)) rot = 0;
    return rot;
}

} // namespace nicxlive::core::nodes
