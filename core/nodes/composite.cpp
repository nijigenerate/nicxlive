#include "composite.hpp"
#include <sstream>

namespace nicxlive::core::nodes {

constexpr uint64_t MaxBoundsResetInterval = 5;

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
        std::stringstream ss(*tinStr);
        if (!(ss >> tint.x >> comma >> tint.y >> comma >> tint.z)) {
            tint = Vec3{};
        }
    } else if (auto tin = data.get_child_optional("tint")) {
        tint.x = tin->get<float>("x", tint.x);
        tint.y = tin->get<float>("y", tint.y);
        tint.z = tin->get<float>("z", tint.z);
    }
    if (auto stStr = data.get_optional<std::string>("screenTint")) {
        char comma;
        std::stringstream ss(*stStr);
        if (!(ss >> screenTint.x >> comma >> screenTint.y >> comma >> screenTint.z)) {
            screenTint = Vec3{};
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
    Vec2 offset = textureOffset;
    if (!std::isfinite(offset.x) || !std::isfinite(offset.y)) offset = Vec2{0, 0};
    Mat4 invComposite = Mat4::inverse(transform().toMat4());
    Mat4 childLocal = Mat4::multiply(invComposite, child->transform().toMat4());
    Mat4 translate = Mat4::translation(Vec3{-offset.x, -offset.y, 0});
    return Mat4::multiply(translate, childLocal);
}

Mat4 Composite::childCoreMatrix(const std::shared_ptr<Part>& child) const {
    if (!child) return Mat4::identity();
    return child->transform().toMat4();
}

Vec4 Composite::localBoundsFromMatrix(const std::shared_ptr<Part>& child, const Mat4& matrix) const {
    float tx = matrix[0][3];
    float ty = matrix[1][3];
    Vec4 bounds{tx, ty, tx, ty};
    if (!child || child->mesh->vertices.empty()) return bounds;
    auto deform = child->deformation;
    for (std::size_t i = 0; i < child->mesh->vertices.size(); ++i) {
        Vec2 localVertex = child->mesh->vertices[i];
        if (i < deform.size()) {
            localVertex.x += deform.x[i];
            localVertex.y += deform.y[i];
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
    auto frameId = currentProjectableFrame();
    if (useMaxChildrenBounds) {
        if (frameId - maxBoundsStartFrame < MaxBoundsResetInterval) {
            return maxChildrenBounds;
        }
        useMaxChildrenBounds = false;
    }
    if (forceUpdate) {
        for (auto& p : subParts) if (p) p->updateBounds();
    }
    Vec4 bounds{};
    bool useMatrixBounds = autoResizedMesh;
    if (useMatrixBounds) {
        bool hasBounds = false;
        for (auto& part : subParts) {
            if (!part) continue;
            auto childMatrix = childCoreMatrix(part);
            auto childBounds = localBoundsFromMatrix(part, childMatrix);
            if (!hasBounds) {
                bounds = childBounds;
                hasBounds = true;
            } else {
                bounds.x = std::min(bounds.x, childBounds.x);
                bounds.y = std::min(bounds.y, childBounds.y);
                bounds.z = std::max(bounds.z, childBounds.z);
                bounds.w = std::max(bounds.w, childBounds.w);
            }
        }
        if (!hasBounds) {
            auto t = transform();
            bounds = Vec4{t.translation.x, t.translation.y, t.translation.x, t.translation.y};
        }
    } else {
        std::vector<Vec4> bvec;
        for (auto& p : subParts) {
            if (p && p->bounds) bvec.push_back(Vec4{(*p->bounds)[0], (*p->bounds)[1], (*p->bounds)[2], (*p->bounds)[3]});
        }
        bounds = mergeBounds(bvec, Vec4{transform().translation.x, transform().translation.y, transform().translation.x, transform().translation.y});
    }
    if (!useMaxChildrenBounds) {
        maxChildrenBounds = bounds;
        useMaxChildrenBounds = true;
        maxBoundsStartFrame = frameId;
    }
    return bounds;
}

void Composite::enableMaxChildrenBounds(const std::shared_ptr<Node>& target) {
    if (auto d = std::dynamic_pointer_cast<Drawable>(target)) {
        if (!autoResizedMesh) d->updateBounds();
    }
    maxChildrenBounds = getChildrenBounds(true);
    useMaxChildrenBounds = true;
    maxBoundsStartFrame = currentProjectableFrame();
    Vec4 b;
    bool hasB = false;
    if (auto d = std::dynamic_pointer_cast<Drawable>(target)) {
        if (autoResizedMesh) {
            if (auto p = std::dynamic_pointer_cast<Part>(d)) {
                b = localBoundsFromMatrix(p, childCoreMatrix(p));
                hasB = true;
            } else if (d->bounds) {
                b = Vec4{(*d->bounds)[0], (*d->bounds)[1], (*d->bounds)[2], (*d->bounds)[3]};
                hasB = true;
            }
        } else if (d->bounds) {
            b = Vec4{(*d->bounds)[0], (*d->bounds)[1], (*d->bounds)[2], (*d->bounds)[3]};
            hasB = true;
        }
    }
    if (hasB) {
        maxChildrenBounds.x = std::min(maxChildrenBounds.x, b.x);
        maxChildrenBounds.y = std::min(maxChildrenBounds.y, b.y);
        maxChildrenBounds.z = std::max(maxChildrenBounds.z, b.z);
        maxChildrenBounds.w = std::max(maxChildrenBounds.w, b.w);
    }
}

} // namespace nicxlive::core::nodes
