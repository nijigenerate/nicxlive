#include "mesh_group.hpp"

#include "../puppet.hpp"
#include "../render.hpp"
#include "../serde.hpp"
#include "../param/parameter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace nicxlive::core::nodes {

using nicxlive::core::common::transformAssign;
using nicxlive::core::common::transformAdd;

MeshGroup::MeshGroup() {
    requirePreProcessTask();
}

MeshGroup::MeshGroup(const std::shared_ptr<Node>& parent) : MeshGroup() {
    if (parent) {
        setParent(parent);
    }
}

void MeshGroup::releaseTarget(const std::shared_ptr<Node>& target) {
    if (!target) return;
    releaseChildNoRecurse(target);
    auto& ch = childrenRef();
    ch.erase(std::remove(ch.begin(), ch.end(), target), ch.end());
    members.erase(std::remove(members.begin(), members.end(), target->uuid), members.end());
}

void MeshGroup::applyDeformToChildren(const std::vector<std::shared_ptr<core::param::Parameter>>& params, bool recursive) {
    if (dynamic || mesh->indices.empty()) return;
    if (!precalculated) precalculate();
    transform();
    forwardMatrix = transform().toMat4();
    inverseMatrix = globalTransform.toMat4().inverse();

    auto update = [&](const Vec2Array& deformationValues) {
        transformedVertices.resize(vertices.size());
        transformedVertices = vertices;
        transformedVertices += deformationValues;
        for (std::size_t index = 0; index < triangles.size(); ++index) {
            auto p1 = transformedVertices[mesh->indices[index * 3]];
            auto p2 = transformedVertices[mesh->indices[index * 3 + 1]];
            auto p3 = transformedVertices[mesh->indices[index * 3 + 2]];
            Mat3 mat{};
            mat[0][0] = p2.x - p1.x; mat[0][1] = p3.x - p1.x; mat[0][2] = p1.x;
            mat[1][0] = p2.y - p1.y; mat[1][1] = p3.y - p1.y; mat[1][2] = p1.y;
            mat[2][0] = 0.0f;        mat[2][1] = 0.0f;        mat[2][2] = 1.0f;
            triangles[index].transformMatrix = mat * triangles[index].offsetMatrices;
        }
    };

    auto resetOffset = [&](const std::shared_ptr<Node>& node, const auto& selfRef) -> void {
        if (!node) return;
        node->offsetTransform.clear();
        node->offsetSort = 0.0f;
        node->transformChanged();
        for (auto& c : node->childrenRef()) {
            selfRef(c, selfRef);
        }
    };

    for (auto& param : params) {
        if (!param) continue;
        auto selfBinding = std::dynamic_pointer_cast<core::param::DeformationParameterBinding>(
            param->getBinding(shared_from_this(), "deform"));
        if (!selfBinding) continue;

        const auto xCount = param->axisPointCount(0);
        const auto yCount = param->axisPointCount(1);
        if (xCount == 0 || yCount == 0) continue;

        for (std::size_t x = 0; x < xCount; ++x) {
            for (std::size_t y = 0; y < yCount; ++y) {
                if (auto pup = puppetRef()) {
                    if (pup->root) resetOffset(pup->root, resetOffset);
                }

                core::param::Vec2u key{x, y};
                Vec2 ofs{0.0f, 0.0f};
                if (x + 1 == xCount && x > 0) { key.x = x - 1; ofs.x = 1.0f; }
                if (param->isVec2 && y + 1 == yCount && y > 0) { key.y = y - 1; ofs.y = 1.0f; }

                core::param::DeformSlot selfSlot;
                if (selfBinding->isSetAt(core::param::Vec2u{x, y})) {
                    selfSlot = selfBinding->valueAt(core::param::Vec2u{x, y});
                } else {
                    selfSlot = selfBinding->sample(key, ofs);
                }
                update(selfSlot.vertexOffsets);

                auto transfer = [&](const std::shared_ptr<Node>& node, const auto& selfRef) -> void {
                    if (!node) return;
                    auto deformable = std::dynamic_pointer_cast<Deformable>(node);
                    auto composite = std::dynamic_pointer_cast<Composite>(node);
                    bool isComposite = static_cast<bool>(composite);
                    bool shouldTransferTranslate = translateChildren;
                    if (deformable) {
                        static const char* kTransformBindingNames[] = {
                            "transform.t.x",
                            "transform.t.y",
                            "transform.r.z",
                            "transform.s.x",
                            "transform.s.y",
                        };
                        for (const char* bindingName : kTransformBindingNames) {
                            for (auto& kv : param->bindingMap) {
                                auto& binding = kv.second;
                                if (!binding) continue;
                                if (binding->getName() != bindingName) continue;
                                if (binding->getNodeUUID() != node->uuid) continue;
                                binding->apply(key, ofs);
                            }
                        }
                        node->transformChanged();

                        auto nodeBinding = std::dynamic_pointer_cast<core::param::DeformationParameterBinding>(
                            param->getOrAddBinding(node, "deform"));
                        if (nodeBinding) {
                            Vec2Array nodeDeform = nodeBinding->valueAt(core::param::Vec2u{x, y}).vertexOffsets;
                            auto matrix = node->transform().toMat4();
                            auto filtered = filterChildren(node, deformable->vertices, nodeDeform, &matrix);
                            const auto& outDeform = std::get<0>(filtered);
                            if (outDeform.size() > 0) {
                                nodeBinding->update(core::param::Vec2u{x, y}, outDeform);
                            }
                        }
                    } else if (shouldTransferTranslate && !isComposite) {
                        Vec2Array oneVertex;
                        oneVertex.resize(1);
                        oneVertex.xAt(0) = node->localTransform.translation.x;
                        oneVertex.yAt(0) = node->localTransform.translation.y;

                        auto parentNode = node->parent.lock();
                        Mat4 matrix = parentNode ? parentNode->transform().toMat4() : Mat4::identity();

                        auto nodeBindingX = std::dynamic_pointer_cast<core::param::ValueParameterBinding>(
                            param->getOrAddBinding(node, "transform.t.x"));
                        auto nodeBindingY = std::dynamic_pointer_cast<core::param::ValueParameterBinding>(
                            param->getOrAddBinding(node, "transform.t.y"));
                        Vec2Array nodeDeform;
                        nodeDeform.resize(1);
                        nodeDeform.xAt(0) = node->offsetTransform.translation.x;
                        nodeDeform.yAt(0) = node->offsetTransform.translation.y;

                        auto filtered = filterChildren(node, oneVertex, nodeDeform, &matrix);
                        const auto& outDeform = std::get<0>(filtered);
                        if (outDeform.size() > 0 && nodeBindingX && nodeBindingY) {
                            float curX = nodeBindingX->valueAt(core::param::Vec2u{x, y});
                            float curY = nodeBindingY->valueAt(core::param::Vec2u{x, y});
                            nodeBindingX->update(core::param::Vec2u{x, y}, curX + outDeform.xAt(0));
                            nodeBindingY->update(core::param::Vec2u{x, y}, curY + outDeform.yAt(0));
                        }
                    }

                    if (recursive && node->mustPropagate()) {
                        for (auto& c : node->childrenRef()) selfRef(c, selfRef);
                    }
                };

                for (auto& c : children) transfer(c, transfer);
            }
        }
        param->removeBinding(selfBinding);
    }

    // After deform transfer, clear own mesh and disable translateChildren like D 実装
    *mesh = MeshData{};
    rebuffer(*mesh);
    translateChildren = false;
    precalculated = false;
}

void MeshGroup::addChild(const std::shared_ptr<Node>& child) {
    Drawable::addChild(child);
    if (child) members.push_back(child->uuid);
}

void MeshGroup::clearChildren() {
    members.clear();
    Drawable::clearChildren();
}

void MeshGroup::setParent(const std::shared_ptr<Node>& p) {
    owner = p;
    Node::setParent(p);
}

std::shared_ptr<Node> MeshGroup::parentPtr() const { return owner.lock(); }

void MeshGroup::preProcess() { Drawable::preProcess(); }
void MeshGroup::postProcess(int id) { Drawable::postProcess(id); }

void MeshGroup::runPreProcessTask(core::RenderContext& ctx) {
    Drawable::runPreProcessTask(ctx);
    if (mesh->indices.empty()) return;
    if (!precalculated) precalculate();
    // update transformedVertices and triangle matrices
    transformedVertices = vertices;
    transformedVertices += deformation;
    for (std::size_t i = 0; i + 2 < mesh->indices.size() && i / 3 < triangles.size(); ++i) {
        auto i0 = mesh->indices[i];
        auto i1 = mesh->indices[i + 1];
        auto i2 = mesh->indices[i + 2];
        Vec2 p1{transformedVertices.xAt(i0), transformedVertices.yAt(i0)};
        Vec2 p2{transformedVertices.xAt(i1), transformedVertices.yAt(i1)};
        Vec2 p3{transformedVertices.xAt(i2), transformedVertices.yAt(i2)};
        Mat3 mat{};
        mat[0][0] = p2.x - p1.x; mat[0][1] = p3.x - p1.x; mat[0][2] = p1.x;
        mat[1][0] = p2.y - p1.y; mat[1][1] = p3.y - p1.y; mat[1][2] = p1.y;
        mat[2][0] = 0; mat[2][1] = 0; mat[2][2] = 1;
        triangles[i / 3].transformMatrix = mat * triangles[i / 3].offsetMatrices;
    }
    forwardMatrix = transform().toMat4();
    inverseMatrix = globalTransform.toMat4().inverse();
}

void MeshGroup::runRenderTask(core::RenderContext&) {
    // MeshGroup does not emit GPU commands; children handle drawing.
}

void MeshGroup::draw() { Drawable::draw(); }

void MeshGroup::renderMask(bool dodge) {
    for (auto& c : children) {
        if (auto p = std::dynamic_pointer_cast<Part>(c)) {
            p->renderMask(dodge);
        }
    }
}

void MeshGroup::rebuffer(const MeshData& data) {
    // D parity: MeshGroup.rebuffer calls Drawable.rebuffer so shared vertex/uv atlases are resized together.
    Drawable::rebufferMesh(data);
    if (dynamic) {
        precalculated = false;
    }
}

void MeshGroup::switchMode(bool dyn) {
    if (dynamic != dyn) {
        dynamic = dyn;
        precalculated = false;
    }
}

void MeshGroup::setTranslateChildren(bool value) {
    if (translateChildren == value) return;
    translateChildren = value;
    for (auto& c : children) {
        setupChild(c);
    }
}

std::tuple<Vec2Array, std::optional<Mat4>, bool> MeshGroup::filterChildren(const std::shared_ptr<Node>& target,
                                                                          const Vec2Array& origVertices,
                                                                          const Vec2Array& origDeformation,
                                                                          const Mat4* origTransform) {
    if (!precalculated || !origTransform) return {Vec2Array{}, std::nullopt, false};
    if (auto deformer = std::dynamic_pointer_cast<Deformer>(target)) {
        if (std::dynamic_pointer_cast<PathDeformer>(deformer)) {
            auto pd = std::dynamic_pointer_cast<PathDeformer>(deformer);
            if (pd && !pd->physicsEnabled) return {Vec2Array{}, std::nullopt, false};
        } else if (!std::dynamic_pointer_cast<GridDeformer>(deformer)) {
            return {Vec2Array{}, std::nullopt, false};
        }
    }

    Mat4 centerMatrix = Mat4::multiply(inverseMatrix, *origTransform);
    Vec2Array centered;
    transformAssign(centered, origVertices, centerMatrix);
    if (dynamic && origDeformation.size()) {
        transformAdd(centered, origDeformation, centerMatrix);
    }

    Vec2Array mapped = centered;
    auto cx = centered.x;
    auto cy = centered.y;
    auto mx = mapped.x;
    auto my = mapped.y;
    const float minX = bounds.x;
    const float maxX = bounds.z;
    const float minY = bounds.y;
    const float maxY = bounds.w;
    std::size_t maskWidth = static_cast<std::size_t>(std::ceil(bounds.z) - std::floor(bounds.x) + 1);
    std::size_t maskHeight = static_cast<std::size_t>(std::ceil(bounds.w) - std::floor(bounds.y) + 1);
    bool anyChanged = false;
    for (std::size_t i = 0; i < centered.size(); ++i) {
        float vx = cx[i];
        float vy = cy[i];
        float outX = vx, outY = vy;
        if (vx >= minX && vx < maxX && vy >= minY && vy < maxY &&
            maskWidth && maskHeight && !bitMask.empty()) {
            auto localX = static_cast<std::ptrdiff_t>(std::floor(vx - minX));
            auto localY = static_cast<std::ptrdiff_t>(std::floor(vy - minY));
            if (localX >= 0 && localY >= 0) {
                std::size_t maskX = static_cast<std::size_t>(localX);
                std::size_t maskY = static_cast<std::size_t>(localY);
                if (maskX < maskWidth && maskY < maskHeight) {
                    std::size_t maskIndex = maskY * maskWidth + maskX;
                    if (maskIndex < bitMask.size()) {
                        uint16_t bit = bitMask[maskIndex];
                        int triIndex = bit ? (bit - 1) : -1;
        if (triIndex >= 0 && static_cast<std::size_t>(triIndex) < triangles.size()) {
            const auto& m = triangles[triIndex].transformMatrix;
            float nx = m[0][0] * vx + m[0][1] * vy + m[0][2];
            float ny = m[1][0] * vx + m[1][1] * vy + m[1][2];
            outX = nx;
                            outY = ny;
                        }
                    }
                }
            }
        }
        mx[i] = outX;
        my[i] = outY;
        if (outX != vx || outY != vy) anyChanged = true;
    }
    if (!anyChanged) return {origDeformation, std::nullopt, false};
    Vec2Array offsetLocal = mapped;
    offsetLocal -= centered;
    Mat4 inv = centerMatrix.inverse();
    inv[0][3] = inv[1][3] = inv[2][3] = 0.0f;
    Vec2Array out = origDeformation;
    transformAdd(out, offsetLocal, inv);
    float maxAbs = 0.0f;
    for (std::size_t i = 0; i < out.size(); ++i) {
        maxAbs = std::max(maxAbs, std::max(std::fabs(out.xAt(i)), std::fabs(out.yAt(i))));
    }
    if (maxAbs > 10.0f) {
        std::fprintf(stderr,
                     "[nicxlive][MeshGroup][LargeOffset] node=%s target=%s targetUuid=%u maxAbs=%.6f first=(%.6f,%.6f)\n",
                     name.c_str(),
                     target ? target->name.c_str() : "<null>",
                     target ? target->uuid : 0u,
                     maxAbs,
                     out.size() ? out.xAt(0) : 0.0f,
                     out.size() ? out.yAt(0) : 0.0f);
    }
    return {out, std::nullopt, true};
}

void MeshGroup::precalculate() {
    if (mesh->indices.empty()) {
        triangles.clear();
        bitMask.clear();
        precalculated = false;
        return;
    }

    auto getBounds = [](const std::vector<Vec2>& verts) {
        Vec4 b{std::numeric_limits<float>::max(),
               std::numeric_limits<float>::max(),
               -std::numeric_limits<float>::max(),
               -std::numeric_limits<float>::max()};
        for (const auto& v : verts) {
            b.x = std::min(b.x, v.x);
            b.y = std::min(b.y, v.y);
            b.z = std::max(b.z, v.x);
            b.w = std::max(b.w, v.y);
        }
        b.x = std::floor(b.x);
        b.y = std::floor(b.y);
        b.z = std::ceil(b.z);
        b.w = std::ceil(b.w);
        return b;
    };

    bounds = getBounds(mesh->vertices);
    triangles.clear();
    std::size_t triCount = mesh->indices.size() / 3;
    triangles.reserve(triCount);

    for (std::size_t i = 0; i < triCount; ++i) {
        auto i0 = mesh->indices[i * 3];
        auto i1 = mesh->indices[i * 3 + 1];
        auto i2 = mesh->indices[i * 3 + 2];
        Vec2 p1 = mesh->vertices[i0];
        Vec2 p2 = mesh->vertices[i1];
        Vec2 p3 = mesh->vertices[i2];

        Vec2 axis0{p2.x - p1.x, p2.y - p1.y};
        float axis0len = std::sqrt(axis0.x * axis0.x + axis0.y * axis0.y);
        if (axis0len != 0.0f) {
            axis0.x /= axis0len;
            axis0.y /= axis0len;
        }
        Vec2 axis1{p3.x - p1.x, p3.y - p1.y};
        float axis1len = std::sqrt(axis1.x * axis1.x + axis1.y * axis1.y);
        if (axis1len != 0.0f) {
            axis1.x /= axis1len;
            axis1.y /= axis1len;
        }

        Vec2 rotated{axis1.x * axis0.x + axis1.y * (-axis0.y), axis1.x * axis0.y + axis1.y * axis0.x};
        float cosA = rotated.x;
        float sinA = rotated.y;

        TriangleMapping t{};
        Mat3 scale{};
        scale[0][0] = axis0len > 0 ? 1.0f / axis0len : 0.0f;
        scale[1][1] = axis1len > 0 ? 1.0f / axis1len : 0.0f;
        Mat3 shear{};
        shear[0][0] = 1.0f;
        shear[0][1] = (sinA != 0.0f) ? -cosA / sinA : 0.0f;
        shear[1][0] = 0.0f;
        shear[1][1] = (sinA != 0.0f) ? 1.0f / sinA : 0.0f;
        Mat3 rot{};
        rot[0][0] = axis0.x;
        rot[0][1] = axis0.y;
        rot[1][0] = -axis0.y;
        rot[1][1] = axis0.x;
        Mat3 translate{};
        translate[0][2] = -p1.x;
        translate[1][2] = -p1.y;

        t.offsetMatrices = scale * shear * rot * translate;
        triangles.push_back(t);
    }

    int width = static_cast<int>(std::ceil(bounds.z) - std::floor(bounds.x) + 1);
    int height = static_cast<int>(std::ceil(bounds.w) - std::floor(bounds.y) + 1);
    bitMask.assign(static_cast<std::size_t>(width * height), 0);

    for (std::size_t idx = 0; idx < triangles.size(); ++idx) {
        auto i0 = mesh->indices[idx * 3];
        auto i1 = mesh->indices[idx * 3 + 1];
        auto i2 = mesh->indices[idx * 3 + 2];
        Vec2 p1 = mesh->vertices[i0];
        Vec2 p2 = mesh->vertices[i1];
        Vec2 p3 = mesh->vertices[i2];
        Vec4 tbounds = getBounds({p1, p2, p3});
        int bwidth = static_cast<int>(std::ceil(tbounds.z) - std::floor(tbounds.x) + 1);
        int bheight = static_cast<int>(std::ceil(tbounds.w) - std::floor(tbounds.y) + 1);
        int top = static_cast<int>(std::floor(tbounds.y));
        int left = static_cast<int>(std::floor(tbounds.x));
        for (int y = 0; y < bheight; ++y) {
            for (int x = 0; x < bwidth; ++x) {
                Vec2 pt{static_cast<float>(left + x), static_cast<float>(top + y)};
                if (pointInTriangle(pt, p1, p2, p3)) {
                    pt.x -= bounds.x;
                    pt.y -= bounds.y;
                    std::size_t maskIndex = static_cast<std::size_t>(pt.y) * static_cast<std::size_t>(width) +
                                             static_cast<std::size_t>(pt.x);
                    if (maskIndex < bitMask.size()) bitMask[maskIndex] = static_cast<uint16_t>(idx + 1);
                }
            }
        }
    }

    precalculated = true;
    for (auto& child : children) {
        setupChild(child);
    }
}

bool MeshGroup::setupChild(const std::shared_ptr<Node>& child) {
    Drawable::setupChild(child);
    if (!child) return false;
    auto visit = [&](const std::shared_ptr<Node>& n, const auto& selfRef) -> void {
        setupChildNoRecurse(n);
        if (n->mustPropagate()) {
            for (auto& c : n->childrenRef()) selfRef(c, selfRef);
        }
    };
    if (!mesh->indices.empty()) {
        visit(child, visit);
    }
    return false;
}

bool MeshGroup::releaseChild(const std::shared_ptr<Node>& child) {
    auto visit = [&](const std::shared_ptr<Node>& n, const auto& selfRef) -> void {
        releaseChildNoRecurse(n);
        if (n->mustPropagate()) {
            for (auto& c : n->childrenRef()) selfRef(c, selfRef);
        }
    };
    visit(child, visit);
    Drawable::releaseChild(child);
    return false;
}

void MeshGroup::setupSelf() {
    Drawable::setupSelf();
    if (!precalculated && !mesh->indices.empty()) {
        precalculate();
    }
}

void MeshGroup::clearCache() {
    precalculated = false;
    bitMask.clear();
    triangles.clear();
}

void MeshGroup::centralize() {
    // D版: まず子を中央寄せ（Drawable::centralize 相当）、その後自身の頂点と子のローカル平行移動を調整
    for (auto& c : children) {
        if (c) c->centralize();
    }
    updateBounds();

    Vec4 bounds{};
    std::vector<Vec3> childWorld;
    if (!children.empty()) {
        bool first = true;
        for (auto& c : children) {
            if (!c) continue;
            auto arr = c->getCombinedBounds();
            Vec4 cb{arr[0], arr[1], arr[2], arr[3]};
            if (first) {
                bounds = cb;
                first = false;
            } else {
                bounds.x = std::min(bounds.x, cb.x);
                bounds.y = std::min(bounds.y, cb.y);
                bounds.z = std::max(bounds.z, cb.z);
                bounds.w = std::max(bounds.w, cb.w);
            }
            childWorld.push_back(c->transform().toMat4().transformPoint(Vec3{0, 0, 1}));
        }
        if (first) {
            auto tr = transform();
            bounds = Vec4{tr.translation.x, tr.translation.y, tr.translation.x, tr.translation.y};
        }
    } else {
        auto tr = transform();
        bounds = Vec4{tr.translation.x, tr.translation.y, tr.translation.x, tr.translation.y};
    }

    Vec2 center{(bounds.x + bounds.z) * 0.5f, (bounds.y + bounds.w) * 0.5f};
    if (auto p = parentPtr()) {
        auto inv = p->transform().toMat4().inverse();
        auto ct = inv.transformPoint(Vec3{center.x, center.y, 1});
        center = Vec2{ct.x, ct.y};
    }
    Vec2 diff{center.x - localTransform.translation.x, center.y - localTransform.translation.y};
    for (auto& v : vertices.x) v -= diff.x;
    for (auto& v : vertices.y) v -= diff.y;
    localTransform.translation.x = center.x;
    localTransform.translation.y = center.y;
    transformChanged();
    clearCache();
    updateBounds();
    auto invSelf = transform().toMat4().inverse();
    for (std::size_t i = 0; i < children.size() && i < childWorld.size(); ++i) {
        auto local = invSelf.transformPoint(childWorld[i]);
        children[i]->localTransform.translation.x = local.x;
        children[i]->localTransform.translation.y = local.y;
        children[i]->transformChanged();
    }
    clearCache();
}

void MeshGroup::copyFrom(const Node& src, bool clone, bool deepCopy) {
    Drawable::copyFrom(src, clone, deepCopy);
    if (auto mg = dynamic_cast<const MeshGroup*>(&src)) {
        bitMask = mg->bitMask;
        bounds = mg->bounds;
        triangles = mg->triangles;
        transformedVertices = mg->transformedVertices;
        forwardMatrix = mg->forwardMatrix;
        inverseMatrix = mg->inverseMatrix;
        translateChildren = mg->translateChildren;
        dynamic = mg->dynamic;
        precalculated = mg->precalculated;
    }
}

void MeshGroup::build(bool force) {
    if (force || !precalculated) {
        precalculate();
    }
    for (auto& c : children) {
        setupChild(c);
    }
    Drawable::build(force);
}

void MeshGroup::serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const {
    Drawable::serializeSelfImpl(serializer, recursive, flags);
    if (has_flag(flags, SerializeNodeFlags::State)) {
        serializer.putKey("dynamic_deformation");
        serializer.putValue(dynamic);
        serializer.putKey("translate_children");
        serializer.putValue(translateChildren);
    }
}

::nicxlive::core::serde::SerdeException MeshGroup::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    if (auto err = Drawable::deserializeFromFghj(data)) return err;
    if (auto dyn = data.get_child_optional("dynamic_deformation")) {
        dynamic = dyn->get_value<bool>();
    }
    if (auto tc = data.get_child_optional("translate_children")) {
        translateChildren = tc->get_value<bool>();
    }
    precalculated = false;
    return std::nullopt;
}

void MeshGroup::setupChildNoRecurse(const std::shared_ptr<Node>& node, bool prepend) {
    if (!node) return;
    auto deformable = std::dynamic_pointer_cast<Deformable>(node);
    bool isDeformable = static_cast<bool>(deformable);
    Node::FilterHook hook{};
    hook.stage = kMeshGroupFilterStage;
    hook.tag = reinterpret_cast<std::uintptr_t>(this);
    hook.func = [this](auto t, auto v, auto d, auto mat) {
                                                     Vec2Array verts; verts.resize(v.size());
                                                     for (std::size_t i = 0; i < v.size(); ++i) { verts.xAt(i) = v[i].x; verts.yAt(i) = v[i].y; }
                                                     Vec2Array def; def.resize(d.size());
                                                     for (std::size_t i = 0; i < d.size(); ++i) { def.xAt(i) = d[i].x; def.yAt(i) = d[i].y; }
                                                     auto res = filterChildren(t, verts, def, mat);
                                                     std::vector<Vec2> out;
                                                     const auto& defOut = std::get<0>(res);
                                                     out.reserve(defOut.size());
                                                     for (std::size_t i = 0; i < defOut.size(); ++i) out.push_back(Vec2{defOut.xAt(i), defOut.yAt(i)});
                                                     return std::make_tuple(out, std::get<1>(res), std::get<2>(res));
                                                  };
    auto& pre = node->preProcessFilters;
    auto& post = node->postProcessFilters;
    const auto tag = reinterpret_cast<std::uintptr_t>(this);
    pre.erase(std::remove_if(pre.begin(), pre.end(), [&](const auto& h) { return h.stage == kMeshGroupFilterStage && h.tag == tag; }), pre.end());
    post.erase(std::remove_if(post.begin(), post.end(), [&](const auto& h) { return h.stage == kMeshGroupFilterStage && h.tag == tag; }), post.end());

    if (isDeformable && dynamic) {
        if (prepend) post.insert(post.begin(), hook); else post.push_back(hook);
    } else if (translateChildren || isDeformable) {
        if (prepend) pre.insert(pre.begin(), hook); else pre.push_back(hook);
    }
}

void MeshGroup::releaseChildNoRecurse(const std::shared_ptr<Node>& node) {
    if (!node) return;
    auto& pre = node->preProcessFilters;
    auto& post = node->postProcessFilters;
    const auto tag = reinterpret_cast<std::uintptr_t>(this);
    pre.erase(std::remove_if(pre.begin(), pre.end(), [&](const auto& h) { return h.stage == kMeshGroupFilterStage && h.tag == tag; }), pre.end());
    post.erase(std::remove_if(post.begin(), post.end(), [&](const auto& h) { return h.stage == kMeshGroupFilterStage && h.tag == tag; }), post.end());
}

bool MeshGroup::pointInTriangle(const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c) {
    auto sign = [](const Vec2& p1, const Vec2& p2, const Vec2& p3) {
        return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
    };
    bool b1 = sign(p, a, b) < 0.0f;
    bool b2 = sign(p, b, c) < 0.0f;
    bool b3 = sign(p, c, a) < 0.0f;
    return ((b1 == b2) && (b2 == b3));
}

} // namespace nicxlive::core::nodes
