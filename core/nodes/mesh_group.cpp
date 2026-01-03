#include "mesh_group.hpp"

#include "../render.hpp"
#include "../serde.hpp"
#include "../param/parameter.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace nicxlive::core::nodes {

MeshGroup::MeshGroup() {
    requirePreProcessTask();
    requirePostTask(0);
}

void MeshGroup::releaseTarget(const std::shared_ptr<Node>& target) {
    if (!target) return;
    releaseChildNoRecurse(target);
    auto& ch = childrenRef();
    ch.erase(std::remove(ch.begin(), ch.end(), target), ch.end());
    members.erase(std::remove(members.begin(), members.end(), target->uuid), members.end());
}

void MeshGroup::applyDeformToChildren(const std::vector<std::shared_ptr<core::param::Parameter>>& params, bool recursive) {
    if (dynamic || mesh.indices.empty()) return;
    if (!precalculated) precalculate();
    forwardMatrix = transform().toMat4();
    inverseMatrix = forwardMatrix.inverse();

    // Traverse children and apply deformation from parameter bindings, then map through MeshGroup filter.
    std::function<void(const std::shared_ptr<Node>&)> apply;
    apply = [&](const std::shared_ptr<Node>& n) {
        if (!n) return;
        auto deformable = std::dynamic_pointer_cast<Deformable>(n);
        if (deformable) {
            // 1) pull deformation from params if binding exists
            for (auto& param : params) {
                if (!param) continue;
                auto binding = std::dynamic_pointer_cast<core::param::DeformationParameterBinding>(param->getBinding(n, "deform"));
                if (!binding) continue;
                uint32_t xCount = param->axisPointCount(0);
                uint32_t yCount = param->axisPointCount(1);
                if (yCount == 0) yCount = 1;
                core::param::Vec2u chosen{0, 0};
                bool found = false;
                for (uint32_t x = 0; x < std::max<uint32_t>(1, xCount); ++x) {
                    for (uint32_t y = 0; y < std::max<uint32_t>(1, yCount); ++y) {
                        core::param::Vec2u idx{x, y};
                        if (binding->isSetAt(idx)) {
                            chosen = idx;
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
                if (found) {
                    auto slot = binding->valueAt(chosen);
                    if (slot.vertexOffsets.size() == deformable->deformation.size()) {
                        deformable->deformation = slot.vertexOffsets;
                        deformable->transformChanged();
                    }
                }
            }

            // 2) apply MeshGroup mapping
            auto m = n->transform().toMat4();
            auto res = filterChildren(n, deformable->vertices, deformable->deformation, &m);
            const auto& deformOut = std::get<0>(res);
            bool changed = std::get<2>(res);
            if (changed && deformOut.size() == deformable->deformation.size()) {
                deformable->deformation = deformOut;
                deformable->updateBounds();
                deformable->transformChanged();
            }
        }
        if (recursive && n->mustPropagate()) {
            for (auto& c : n->childrenRef()) apply(c);
        }
    };
    for (auto& c : children) apply(c);

    // After deform transfer, clear own mesh and disable translateChildren like D 実装
    mesh = MeshData{};
    rebuffer(mesh);
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

void MeshGroup::runPreProcessTask(core::RenderContext&) {
    if (mesh.indices.empty()) return;
    if (!precalculated) precalculate();
    // update transformedVertices and triangle matrices
    transformedVertices = vertices;
    transformedVertices += deformation;
    for (std::size_t i = 0; i + 2 < mesh.indices.size() && i / 3 < triangles.size(); ++i) {
        auto i0 = mesh.indices[i];
        auto i1 = mesh.indices[i + 1];
        auto i2 = mesh.indices[i + 2];
        Vec2 p1{transformedVertices.x[i0], transformedVertices.y[i0]};
        Vec2 p2{transformedVertices.x[i1], transformedVertices.y[i1]};
        Vec2 p3{transformedVertices.x[i2], transformedVertices.y[i2]};
        Mat3 mat{};
        mat[0][0] = p2.x - p1.x; mat[0][1] = p3.x - p1.x; mat[0][2] = p1.x;
        mat[1][0] = p2.y - p1.y; mat[1][1] = p3.y - p1.y; mat[1][2] = p1.y;
        mat[2][0] = 0; mat[2][1] = 0; mat[2][2] = 1;
        triangles[i / 3].transformMatrix = mat * triangles[i / 3].offsetMatrices;
    }
    forwardMatrix = transform().toMat4();
    inverseMatrix = forwardMatrix.inverse();
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
    mesh = data;
    vertices.clear();
    deformation.clear();
    vertices.resize(mesh.vertices.size());
    deformation.resize(mesh.vertices.size());
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        vertices.x[i] = mesh.vertices[i].x;
        vertices.y[i] = mesh.vertices[i].y;
    }
    precalculated = false;
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
            if (pd && !pd->renderEnabled()) return {Vec2Array{}, std::nullopt, false};
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
    return {out, std::nullopt, true};
}

void MeshGroup::precalculate() {
    if (mesh.indices.empty()) {
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

    bounds = getBounds(mesh.vertices);
    triangles.clear();
    std::size_t triCount = mesh.indices.size() / 3;
    triangles.reserve(triCount);

    for (std::size_t i = 0; i < triCount; ++i) {
        auto i0 = mesh.indices[i * 3];
        auto i1 = mesh.indices[i * 3 + 1];
        auto i2 = mesh.indices[i * 3 + 2];
        Vec2 p1 = mesh.vertices[i0];
        Vec2 p2 = mesh.vertices[i1];
        Vec2 p3 = mesh.vertices[i2];

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
        auto i0 = mesh.indices[idx * 3];
        auto i1 = mesh.indices[idx * 3 + 1];
        auto i2 = mesh.indices[idx * 3 + 2];
        Vec2 p1 = mesh.vertices[i0];
        Vec2 p2 = mesh.vertices[i1];
        Vec2 p3 = mesh.vertices[i2];
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
    if (!mesh.indices.empty()) {
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
    if (!precalculated && !mesh.indices.empty()) {
        precalculate();
    }
}

void MeshGroup::clearCache() {
    precalculated = false;
    bitMask.clear();
    triangles.clear();
}

void MeshGroup::centralize() {
    if (!children.empty()) {
        Vec4 b{};
        bool first = true;
        std::vector<Vec3> childWorld;
        for (auto& c : children) {
            auto arr = c->getCombinedBounds();
            Vec4 cb{arr[0], arr[1], arr[2], arr[3]};
            if (first) {
                b = cb;
                first = false;
            } else {
                b.x = std::min(b.x, cb.x);
                b.y = std::min(b.y, cb.y);
                b.z = std::max(b.z, cb.z);
                b.w = std::max(b.w, cb.w);
            }
            childWorld.push_back(c->transform().toMat4().transformPoint(Vec3{0, 0, 1}));
        }
        Vec2 center{(b.x + b.z) * 0.5f, (b.y + b.w) * 0.5f};
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
        auto inv = transform().toMat4().inverse();
        for (std::size_t i = 0; i < children.size() && i < childWorld.size(); ++i) {
            auto local = inv.transformPoint(childWorld[i]);
            children[i]->localTransform.translation.x = local.x;
            children[i]->localTransform.translation.y = local.y;
            children[i]->transformChanged();
        }
    } else {
        centralizeDrawable();
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
    auto hook = Node::FilterHook{kMeshGroupFilterStage, [this](auto t, auto v, auto d, auto mat) {
                                                     Vec2Array verts; verts.resize(v.size());
                                                     for (std::size_t i = 0; i < v.size(); ++i) { verts.x[i] = v[i].x; verts.y[i] = v[i].y; }
                                                     Vec2Array def; def.resize(d.size());
                                                     for (std::size_t i = 0; i < d.size(); ++i) { def.x[i] = d[i].x; def.y[i] = d[i].y; }
                                                     auto res = filterChildren(t, verts, def, mat);
                                                     std::vector<Vec2> out;
                                                     const auto& defOut = std::get<0>(res);
                                                     out.reserve(defOut.size());
                                                     for (std::size_t i = 0; i < defOut.size(); ++i) out.push_back(Vec2{defOut.x[i], defOut.y[i]});
                                                     return std::make_tuple(out, std::get<1>(res), std::get<2>(res));
                                                  }};
    auto& pre = node->preProcessFilters;
    auto& post = node->postProcessFilters;
    pre.erase(std::remove_if(pre.begin(), pre.end(), [&](const auto& h) { return h.stage == kMeshGroupFilterStage; }), pre.end());
    post.erase(std::remove_if(post.begin(), post.end(), [&](const auto& h) { return h.stage == kMeshGroupFilterStage; }), post.end());

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
    pre.erase(std::remove_if(pre.begin(), pre.end(), [&](const auto& h) { return h.stage == kMeshGroupFilterStage; }), pre.end());
    post.erase(std::remove_if(post.begin(), post.end(), [&](const auto& h) { return h.stage == kMeshGroupFilterStage; }), post.end());
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
