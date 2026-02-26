#include "mesh_group.hpp"

#include "../puppet.hpp"
#include "../render.hpp"
#include "../serde.hpp"
#include "../param/parameter.hpp"
#include "../debug_log.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <unordered_map>

namespace nicxlive::core::nodes {

using nicxlive::core::common::transformAssign;
using nicxlive::core::common::transformAdd;

namespace {
bool invertMat3(const Mat3& in, Mat3& out) {
    const float a00 = in.m[0][0], a01 = in.m[0][1], a02 = in.m[0][2];
    const float a10 = in.m[1][0], a11 = in.m[1][1], a12 = in.m[1][2];
    const float a20 = in.m[2][0], a21 = in.m[2][1], a22 = in.m[2][2];
    const float c00 = (a11 * a22 - a12 * a21);
    const float c01 = (a12 * a20 - a10 * a22);
    const float c02 = (a10 * a21 - a11 * a20);
    const float c10 = (a02 * a21 - a01 * a22);
    const float c11 = (a00 * a22 - a02 * a20);
    const float c12 = (a01 * a20 - a00 * a21);
    const float c20 = (a01 * a12 - a02 * a11);
    const float c21 = (a02 * a10 - a00 * a12);
    const float c22 = (a00 * a11 - a01 * a10);
    const float det = a00 * c00 + a01 * c01 + a02 * c02;
    if (std::fabs(det) <= 1e-12f) {
        return false;
    }
    const float invDet = 1.0f / det;
    out.m[0][0] = c00 * invDet;
    out.m[0][1] = c10 * invDet;
    out.m[0][2] = c20 * invDet;
    out.m[1][0] = c01 * invDet;
    out.m[1][1] = c11 * invDet;
    out.m[1][2] = c21 * invDet;
    out.m[2][0] = c02 * invDet;
    out.m[2][1] = c12 * invDet;
    out.m[2][2] = c22 * invDet;
    return true;
}

bool traceMeshGroupChainEnabled() {
    static int enabled = -1;
    if (enabled >= 0) return enabled != 0;
    const char* v = std::getenv("NJCX_TRACE_MESHGROUP_CHAIN");
    if (!v) {
        enabled = 0;
        return false;
    }
    enabled = (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 || std::strcmp(v, "TRUE") == 0) ? 1 : 0;
    return enabled != 0;
}

bool traceMeshGroupEffectEnabled() {
    static int enabled = -1;
    if (enabled >= 0) return enabled != 0;
    const char* v = std::getenv("NJCX_TRACE_MESHGROUP_EFFECT");
    if (!v) {
        enabled = 0;
        return false;
    }
    enabled = (std::strcmp(v, "1") == 0 || std::strcmp(v, "true") == 0 || std::strcmp(v, "TRUE") == 0) ? 1 : 0;
    return enabled != 0;
}

const char* meshGroupTargetTraceName() {
    static const char* cached = nullptr;
    static bool init = false;
    if (!init) {
        cached = std::getenv("NJCX_TRACE_MESHGROUP_TARGET");
        init = true;
    }
    return cached;
}

std::unordered_map<const MeshGroup*, std::size_t> gMeshGroupFilterCallCount;
} // namespace

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

    applyDeformToChildrenInternal(
        std::dynamic_pointer_cast<Node>(shared_from_this()),
        [this](std::shared_ptr<Node> target, const Vec2Array& verticesIn, Vec2Array deformationIn, const Mat4* transformIn) {
            return filterChildren(target, verticesIn, deformationIn, transformIn);
        },
        update,
        [this]() { return translateChildren; },
        params,
        recursive);

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
    if (traceMeshGroupChainEnabled()) {
        gMeshGroupFilterCallCount[this] = 0;
    }
    if (mesh->indices.empty()) return;
    if (!precalculated) precalculate();
    // update transformedVertices and triangle matrices
    transformedVertices = vertices;
    transformedVertices += deformation;
    if (traceMeshGroupEffectEnabled()) {
        float deformMaxAbs = 0.0f;
        for (std::size_t i = 0; i < deformation.size(); ++i) {
            deformMaxAbs = std::max(deformMaxAbs, std::max(std::fabs(deformation.xAt(i)), std::fabs(deformation.yAt(i))));
        }
        NJCX_DBG_LOG("[nicxlive][MeshGroup][Pre] node=%s(%u) verts=%zu deform=%zu deformMaxAbs=%.6f precalc=%d\n",
                     name.c_str(),
                     uuid,
                     vertices.size(),
                     deformation.size(),
                     deformMaxAbs,
                     precalculated ? 1 : 0);
    }
    for (std::size_t tri = 0; tri < triangles.size(); ++tri) {
        const std::size_t base = tri * 3;
        if (base + 2 >= mesh->indices.size()) break;
        auto i0 = mesh->indices[base];
        auto i1 = mesh->indices[base + 1];
        auto i2 = mesh->indices[base + 2];
        Vec2 p1{transformedVertices.xAt(i0), transformedVertices.yAt(i0)};
        Vec2 p2{transformedVertices.xAt(i1), transformedVertices.yAt(i1)};
        Vec2 p3{transformedVertices.xAt(i2), transformedVertices.yAt(i2)};
        Mat3 mat{};
        mat[0][0] = p2.x - p1.x; mat[0][1] = p3.x - p1.x; mat[0][2] = p1.x;
        mat[1][0] = p2.y - p1.y; mat[1][1] = p3.y - p1.y; mat[1][2] = p1.y;
        mat[2][0] = 0; mat[2][1] = 0; mat[2][2] = 1;
        triangles[tri].transformMatrix = mat * triangles[tri].offsetMatrices;
    }
    forwardMatrix = transform().toMat4();
    inverseMatrix = globalTransform.toMat4().inverse();

    if (traceMeshGroupChainEnabled()) {
        std::size_t taggedPre = 0;
        std::size_t taggedPost = 0;
        std::size_t taggedNodes = 0;
        std::size_t visited = 0;
        const auto tag = reinterpret_cast<std::uintptr_t>(this);
        auto walk = [&](const std::shared_ptr<Node>& n, const auto& selfRef) -> void {
            if (!n) return;
            ++visited;
            std::size_t localPre = 0;
            std::size_t localPost = 0;
            for (const auto& h : n->preProcessFilters) {
                if (h.stage == kMeshGroupFilterStage && h.tag == tag) ++localPre;
            }
            for (const auto& h : n->postProcessFilters) {
                if (h.stage == kMeshGroupFilterStage && h.tag == tag) ++localPost;
            }
            if (localPre || localPost) ++taggedNodes;
            taggedPre += localPre;
            taggedPost += localPost;
            for (auto& c : n->childrenRef()) {
                selfRef(c, selfRef);
            }
        };
        for (auto& c : children) {
            walk(c, walk);
        }
        const auto calls = gMeshGroupFilterCallCount[this];
        NJCX_DBG_LOG("[nicxlive][MeshGroup][Chain] node=%s uuid=%u visited=%zu taggedNodes=%zu hooks(pre=%zu post=%zu) filterCalls=%zu dynamic=%d translateChildren=%d\n",
                     name.c_str(),
                     uuid,
                     visited,
                     taggedNodes,
                     taggedPre,
                     taggedPost,
                     calls,
                     dynamic ? 1 : 0,
                     translateChildren ? 1 : 0);
    }
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

std::tuple<Vec2Array, Mat4*, bool> MeshGroup::filterChildren(const std::shared_ptr<Node>& target,
                                                             const Vec2Array& origVertices,
                                                             Vec2Array origDeformation,
                                                             const Mat4* origTransform) {
    if (traceMeshGroupChainEnabled()) {
        ++gMeshGroupFilterCallCount[this];
    }
    if (!precalculated || !origTransform) return {Vec2Array{}, nullptr, false};
    if (auto deformer = std::dynamic_pointer_cast<Deformer>(target)) {
        if (std::dynamic_pointer_cast<PathDeformer>(deformer)) {
            auto pd = std::dynamic_pointer_cast<PathDeformer>(deformer);
            if (pd && !pd->physicsEnabled()) return {Vec2Array{}, nullptr, false};
        } else if (!std::dynamic_pointer_cast<GridDeformer>(deformer)) {
            return {Vec2Array{}, nullptr, false};
        }
    }

    Mat4 centerMatrix = Mat4::multiply(inverseMatrix, *origTransform);
    const char* traceTarget = meshGroupTargetTraceName();
    const bool traceThisTarget = traceTarget && target && target->name.find(traceTarget) != std::string::npos;
    Vec2Array centered;
    transformAssign(centered, origVertices, centerMatrix);
    if (dynamic && origDeformation.size()) {
        transformAdd(centered, origDeformation, centerMatrix);
    }

    Vec2Array mapped = centered.dup();
    const float minX = bounds.x;
    const float maxX = bounds.z;
    const float minY = bounds.y;
    const float maxY = bounds.w;
    std::size_t maskWidth = static_cast<std::size_t>(std::ceil(bounds.z) - std::floor(bounds.x) + 1);
    std::size_t maskHeight = static_cast<std::size_t>(std::ceil(bounds.w) - std::floor(bounds.y) + 1);
    bool anyChanged = false;
    std::size_t inBoundsCount = 0;
    std::size_t triHitCount = 0;
    float maxLocalDelta = 0.0f;
    for (std::size_t i = 0; i < centered.size(); ++i) {
        float vx = centered.xAt(i);
        float vy = centered.yAt(i);
        float outX = vx, outY = vy;
        if (vx >= minX && vx < maxX && vy >= minY && vy < maxY &&
            maskWidth && maskHeight && !bitMask.empty()) {
            ++inBoundsCount;
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
                            ++triHitCount;
                            const auto& m = triangles[triIndex].transformMatrix;
                            outX = m[0][0] * vx + m[0][1] * vy + m[0][2];
                            outY = m[1][0] * vx + m[1][1] * vy + m[1][2];
                        }
                    }
                }
            }
        }
        mapped.xAt(i) = outX;
        mapped.yAt(i) = outY;
        const float dx = outX - vx;
        const float dy = outY - vy;
        maxLocalDelta = std::max(maxLocalDelta, std::max(std::fabs(dx), std::fabs(dy)));
        if (outX != vx || outY != vy) anyChanged = true;
    }
    if (traceMeshGroupEffectEnabled()) {
        NJCX_DBG_LOG("[nicxlive][MeshGroup][Effect] group=%s(%u) target=%s(%u) verts=%zu inBounds=%zu triHit=%zu anyChanged=%d maxLocalDelta=%.6f precalc=%d triCount=%zu bitMask=%zu\n",
                     name.c_str(),
                     uuid,
                     target ? target->name.c_str() : "<null>",
                     target ? target->uuid : 0u,
                     centered.size(),
                     inBoundsCount,
                     triHitCount,
                     anyChanged ? 1 : 0,
                     maxLocalDelta,
                     precalculated ? 1 : 0,
                     triangles.size(),
                     bitMask.size());
        if (traceThisTarget && centered.size() > 0) {
            NJCX_DBG_LOG("[nicxlive][MeshGroup][Target] group=%s target=%s centerM=[%.6f %.6f %.6f %.6f | %.6f %.6f %.6f %.6f | %.6f %.6f %.6f %.6f | %.6f %.6f %.6f %.6f] inM=[%.6f %.6f %.6f %.6f]\n",
                         name.c_str(),
                         target->name.c_str(),
                         centerMatrix[0][0], centerMatrix[0][1], centerMatrix[0][2], centerMatrix[0][3],
                         centerMatrix[1][0], centerMatrix[1][1], centerMatrix[1][2], centerMatrix[1][3],
                         centerMatrix[2][0], centerMatrix[2][1], centerMatrix[2][2], centerMatrix[2][3],
                         centerMatrix[3][0], centerMatrix[3][1], centerMatrix[3][2], centerMatrix[3][3],
                         centered.xAt(0), centered.yAt(0), mapped.xAt(0), mapped.yAt(0));
        }
    }
    if (!anyChanged) return {origDeformation, nullptr, false};
    Vec2Array offsetLocal = mapped.dup();
    offsetLocal -= centered;
    Mat4 inv = centerMatrix.inverse();
    inv[0][3] = inv[1][3] = inv[2][3] = 0.0f;
    transformAdd(origDeformation, offsetLocal, inv, offsetLocal.size());
    float maxAbs = 0.0f;
    for (std::size_t i = 0; i < origDeformation.size(); ++i) {
        maxAbs = std::max(maxAbs, std::max(std::fabs(origDeformation.xAt(i)), std::fabs(origDeformation.yAt(i))));
    }
    if (maxAbs > 10.0f) {
        NJCX_DBG_LOG("[nicxlive][MeshGroup][LargeOffset] node=%s target=%s targetUuid=%u maxAbs=%.6f first=(%.6f,%.6f)\n",
                     name.c_str(),
                     target ? target->name.c_str() : "<null>",
                     target ? target->uuid : 0u,
                     maxAbs,
                     origDeformation.size() ? origDeformation.xAt(0) : 0.0f,
                     origDeformation.size() ? origDeformation.yAt(0) : 0.0f);
    }
    return {origDeformation, nullptr, true};
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

        Mat3 base{};
        base[0][0] = p2.x - p1.x;
        base[0][1] = p3.x - p1.x;
        base[0][2] = p1.x;
        base[1][0] = p2.y - p1.y;
        base[1][1] = p3.y - p1.y;
        base[1][2] = p1.y;
        base[2][0] = 0.0f;
        base[2][1] = 0.0f;
        base[2][2] = 1.0f;

        TriangleMapping t{};
        if (!invertMat3(base, t.offsetMatrices)) {
            t.offsetMatrices = Mat3{};
        }
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
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        vertices.xAt(i) -= diff.x;
        vertices.yAt(i) -= diff.y;
    }
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
    translateChildren = false;
    if (auto tc = data.get_child_optional("translate_children")) {
        translateChildren = tc->get_value<bool>();
    }
    precalculated = false;
    return std::nullopt;
}

void MeshGroup::setupChildNoRecurse(const std::shared_ptr<Node>& node, bool prepend) {
    if (!node) return;
    if (auto comp = std::dynamic_pointer_cast<Composite>(node)) {
        if (comp->propagateMeshGroupEnabled()) {
            return;
        }
    }
    auto deformable = std::dynamic_pointer_cast<Deformable>(node);
    bool isDeformable = static_cast<bool>(deformable);
    Node::FilterHook hook{};
    hook.stage = kMeshGroupFilterStage;
    hook.tag = reinterpret_cast<std::uintptr_t>(this);
    hook.func = [this](std::shared_ptr<Node> t,
                       const Vec2Array& verts,
                       Vec2Array def,
                       const Mat4* mat) {
        return filterChildren(t, verts, def, mat);
    };
    auto& pre = node->preProcessFilters;
    auto& post = node->postProcessFilters;
    const auto tag = reinterpret_cast<std::uintptr_t>(this);
    auto removeTagged = [&](std::vector<Node::FilterHook>& dst) {
        dst.erase(std::remove_if(dst.begin(), dst.end(), [&](const auto& h) {
            return h.stage == kMeshGroupFilterStage && h.tag == tag;
        }), dst.end());
    };
    auto upsertTagged = [&](std::vector<Node::FilterHook>& dst) {
        const bool exists = std::any_of(dst.begin(), dst.end(), [&](const auto& h) {
            return h.stage == kMeshGroupFilterStage && h.tag == tag;
        });
        if (!exists) {
            if (prepend) {
                dst.insert(dst.begin(), hook);
            } else {
                dst.push_back(hook);
            }
        }
    };

    if (translateChildren || isDeformable) {
        if (isDeformable && dynamic) {
            removeTagged(pre);
            upsertTagged(post);
        } else {
            upsertTagged(pre);
            removeTagged(post);
        }
    } else {
        removeTagged(pre);
        removeTagged(post);
    }

    if (traceMeshGroupChainEnabled()) {
        std::size_t localPre = 0;
        std::size_t localPost = 0;
        for (const auto& h : pre) {
            if (h.stage == kMeshGroupFilterStage && h.tag == tag) ++localPre;
        }
        for (const auto& h : post) {
            if (h.stage == kMeshGroupFilterStage && h.tag == tag) ++localPost;
        }
        NJCX_DBG_LOG("[nicxlive][MeshGroup][SetupChild] group=%s(%u) target=%s(%u) deformable=%d dynamic=%d translateChildren=%d prepend=%d hooks(pre=%zu post=%zu)\n",
                     name.c_str(),
                     uuid,
                     node ? node->name.c_str() : "<null>",
                     node ? node->uuid : 0u,
                     isDeformable ? 1 : 0,
                     dynamic ? 1 : 0,
                     translateChildren ? 1 : 0,
                     prepend ? 1 : 0,
                     localPre,
                     localPost);
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
    const float d1 = sign(p, a, b);
    const float d2 = sign(p, b, c);
    const float d3 = sign(p, c, a);
    const bool hasNeg = (d1 < 0.0f) || (d2 < 0.0f) || (d3 < 0.0f);
    const bool hasPos = (d1 > 0.0f) || (d2 > 0.0f) || (d3 > 0.0f);
    return !(hasNeg && hasPos);
}

} // namespace nicxlive::core::nodes
