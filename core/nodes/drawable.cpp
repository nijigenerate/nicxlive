#include "drawable.hpp"

#include "node.hpp"
#include "../puppet.hpp"

#include "../serde.hpp"
#include "../render/common.hpp"
#include "../render/shared_deform_buffer.hpp"
#include "../render/commands.hpp"
#include "../math/triangle.hpp"
#include "../math/mat3.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace nicxlive::core::nodes {

namespace {
bool gDoGenerateBounds = false;
constexpr std::uintptr_t kNodeAttachFilterTag = 0x6e617474u; // 'natt'
constexpr std::uintptr_t kWeldFilterTagBase = 0x776c6400u;   // 'wld\0'

using nicxlive::core::math::applyAffine;
using nicxlive::core::math::barycentric;
using nicxlive::core::math::inverse;
using nicxlive::core::math::Mat3x3;
using nicxlive::core::math::multiply;
using nicxlive::core::math::pointInTriangle;
using nicxlive::core::common::gatherVec2;
using nicxlive::core::common::scatterAddVec2;
using nicxlive::core::common::transformAdd;
using nicxlive::core::common::transformAssign;

std::optional<std::array<std::size_t, 3>> findSurroundingTriangle(const Vec2& pt, const MeshData& mesh) {
    if (mesh.indices.size() < 3) return std::nullopt;
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        auto i0 = mesh.indices[i];
        auto i1 = mesh.indices[i + 1];
        auto i2 = mesh.indices[i + 2];
        if (i0 >= mesh.vertices.size() || i1 >= mesh.vertices.size() || i2 >= mesh.vertices.size()) continue;
        Vec2 p0 = mesh.vertices[i0];
        Vec2 p1 = mesh.vertices[i1];
        Vec2 p2 = mesh.vertices[i2];
        if (pointInTriangle(pt, p0, p1, p2)) {
            return std::array<std::size_t, 3>{i0, i1, i2};
        }
    }
    return std::nullopt;
}

bool calculateTransformInTriangle(const MeshData& mesh, const std::array<std::size_t, 3>& tri, const Vec2Array& deform,
                                   const Vec2& target, Vec2& targetPrime, float& rotVert, float& rotHorz) {
    Vec2 p0 = mesh.vertices[tri[0]];
    Vec2 p1 = mesh.vertices[tri[1]];
    Vec2 p2 = mesh.vertices[tri[2]];
    Mat3x3 original{};
    original[0][0] = p0.x; original[0][1] = p1.x; original[0][2] = p2.x;
    original[1][0] = p0.y; original[1][1] = p1.y; original[1][2] = p2.y;
    original[2][0] = 1.0f; original[2][1] = 1.0f; original[2][2] = 1.0f;

    Vec2 p3{p0.x + (tri[0] < deform.size() ? deform.xAt(tri[0]) : 0.0f), p0.y + (tri[0] < deform.size() ? deform.yAt(tri[0]) : 0.0f)};
    Vec2 p4{p1.x + (tri[1] < deform.size() ? deform.xAt(tri[1]) : 0.0f), p1.y + (tri[1] < deform.size() ? deform.yAt(tri[1]) : 0.0f)};
    Vec2 p5{p2.x + (tri[2] < deform.size() ? deform.xAt(tri[2]) : 0.0f), p2.y + (tri[2] < deform.size() ? deform.yAt(tri[2]) : 0.0f)};

    Mat3x3 transformed{};
    transformed[0][0] = p3.x; transformed[0][1] = p4.x; transformed[0][2] = p5.x;
    transformed[1][0] = p3.y; transformed[1][1] = p4.y; transformed[1][2] = p5.y;
    transformed[2][0] = 1.0f; transformed[2][1] = 1.0f; transformed[2][2] = 1.0f;

    Mat3x3 affine = multiply(transformed, inverse(original));
    targetPrime = applyAffine(affine, target);

    Vec2 vert{target.x, target.y + 1.0f};
    Vec2 vertPrime = applyAffine(affine, vert);
    Vec2 horz{target.x + 1.0f, target.y};
    Vec2 horzPrime = applyAffine(affine, horz);

    auto angle = [](const Vec2& a, const Vec2& b) {
        return std::atan2(b.y - a.y, b.x - a.x);
    };

    float originalVert = angle(target, vert);
    float transformedVert = angle(targetPrime, vertPrime);
    rotVert = transformedVert - originalVert;

    float originalHorz = angle(target, horz);
    float transformedHorz = angle(targetPrime, horzPrime);
    rotHorz = transformedHorz - originalHorz;

    return true;
}
} // namespace

Drawable::Drawable() : Deformable() {
    ::nicxlive::core::render::sharedDeformRegister(deformation, &deformOffset);
    ::nicxlive::core::render::sharedVertexRegister(vertices, &vertexOffset);
    ::nicxlive::core::render::sharedUvRegister(sharedUvs, &uvOffset);
}

Drawable::Drawable(const std::shared_ptr<Node>& parent) : Deformable(parent) {
    ::nicxlive::core::render::sharedDeformRegister(deformation, &deformOffset);
    ::nicxlive::core::render::sharedVertexRegister(vertices, &vertexOffset);
    ::nicxlive::core::render::sharedUvRegister(sharedUvs, &uvOffset);
}

Drawable::Drawable(const MeshData& data, uint32_t uuidVal, const std::shared_ptr<Node>& parent)
    : Deformable(uuidVal, parent), mesh(std::make_shared<MeshData>(data)) {
    ::nicxlive::core::render::sharedDeformRegister(deformation, &deformOffset);
    ::nicxlive::core::render::sharedVertexRegister(vertices, &vertexOffset);
    ::nicxlive::core::render::sharedUvRegister(sharedUvs, &uvOffset);
    sharedUvResize(sharedUvs, mesh->uvs.size());
    for (std::size_t i = 0; i < mesh->uvs.size(); ++i) {
        sharedUvs.set(i, mesh->uvs[i]);
    }
    sharedUvMarkDirty();
    updateIndices();
    updateVertices();
}

Drawable::~Drawable() {
    ::nicxlive::core::render::sharedDeformUnregister(deformation);
    ::nicxlive::core::render::sharedVertexUnregister(vertices);
    ::nicxlive::core::render::sharedUvUnregister(sharedUvs);
}

void MeshData::add(const Vec2& vertex, const Vec2& uv) {
    vertices.push_back(vertex);
    uvs.push_back(uv);
}

void MeshData::clearConnections() {
    indices.clear();
}

void MeshData::connect(uint16_t first, uint16_t second) {
    indices.push_back(first);
    indices.push_back(second);
}

int MeshData::find(const Vec2& vert) const {
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        if (vertices[i].x == vert.x && vertices[i].y == vert.y) return static_cast<int>(i);
    }
    return -1;
}

bool MeshData::isReady() const {
    if (indices.empty() || indices.size() % 3 != 0) {
        return false;
    }
    uint16_t maxIdx = 0;
    for (auto v : indices) {
        if (v > maxIdx) maxIdx = v;
    }
    return static_cast<size_t>(maxIdx) < vertices.size();
}

bool MeshData::canTriangulate() const {
    return !indices.empty() && indices.size() % 3 == 0;
}

void MeshData::fixWinding() {
    if (!isReady()) return;
    for (std::size_t j = 0; j + 2 < indices.size(); j += 3) {
        std::size_t i0 = indices[j + 0];
        std::size_t i1 = indices[j + 1];
        std::size_t i2 = indices[j + 2];
        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) continue;
        Vec2 a = vertices[i0];
        Vec2 b = vertices[i1];
        Vec2 c = vertices[i2];
        float crossZ = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        bool cw = crossZ < 0;
        if (cw) {
            std::swap(indices[j + 1], indices[j + 2]);
        }
    }
}

int MeshData::connectionsAtPoint(const Vec2& pt) const {
    int p = find(pt);
    if (p == -1) return 0;
    return connectionsAtPoint(static_cast<uint16_t>(p));
}

int MeshData::connectionsAtPoint(uint16_t idx) const {
    int found = 0;
    for (auto v : indices) {
        if (v == idx) found++;
    }
    return found;
}

MeshData MeshData::copy() const {
    MeshData out;
    out.vertices = vertices;
    out.uvs = uvs;
    out.indices = indices;
    out.gridAxes = gridAxes;
    out.origin = origin;
    return out;
}

Vec2Array& sharedVertexBufferData() { return ::nicxlive::core::render::sharedVertexBufferData(); }
Vec2Array& sharedUvBufferData() { return ::nicxlive::core::render::sharedUvBufferData(); }
Vec2Array& sharedDeformBufferData() { return ::nicxlive::core::render::sharedDeformBufferData(); }

void sharedVertexResize(Vec2Array& target, std::size_t newLength) { ::nicxlive::core::render::sharedVertexResize(target, newLength); }
void sharedUvResize(Vec2Array& target, std::size_t newLength) { ::nicxlive::core::render::sharedUvResize(target, newLength); }
void sharedUvResize(std::vector<Vec2>& target, std::size_t newLength) { target.resize(newLength); }
void sharedDeformResize(Vec2Array& target, std::size_t newLength) { ::nicxlive::core::render::sharedDeformResize(target, newLength); }
void sharedVertexMarkDirty() { ::nicxlive::core::render::sharedVertexMarkDirty(); }
void sharedUvMarkDirty() { ::nicxlive::core::render::sharedUvMarkDirty(); }
void sharedDeformMarkDirty() { ::nicxlive::core::render::sharedDeformMarkDirty(); }
void inSetUpdateBounds(bool state) {
    (void)state;
    gDoGenerateBounds = true;
}
bool inGetUpdateBounds() { return gDoGenerateBounds; }

void MeshData::serialize(::nicxlive::core::serde::InochiSerializer& serializer) const {
    serializer.putKey("verts");
    auto arr = serializer.listBegin();
    for (const auto& v : vertices) {
        serializer.elemBegin();
        serializer.putValue(v.x);
        serializer.elemBegin();
        serializer.putValue(v.y);
    }
    serializer.listEnd(arr);

    if (!uvs.empty()) {
        serializer.putKey("uvs");
        auto uarr = serializer.listBegin();
        for (const auto& uv : uvs) {
            serializer.elemBegin();
            serializer.putValue(uv.x);
            serializer.elemBegin();
            serializer.putValue(uv.y);
        }
        serializer.listEnd(uarr);
    }

    serializer.putKey("indices");
    auto iarr = serializer.listBegin();
    for (auto idx : indices) {
        serializer.elemBegin();
        serializer.putValue(static_cast<uint16_t>(idx));
    }
    serializer.listEnd(iarr);

    serializer.putKey("origin");
    auto oarr = serializer.listBegin();
    serializer.elemBegin();
    serializer.putValue(origin.x);
    serializer.elemBegin();
    serializer.putValue(origin.y);
    serializer.listEnd(oarr);

    if (!gridAxes.empty()) {
        serializer.putKey("grid_axes");
        auto garr = serializer.listBegin();
        for (const auto& axis : gridAxes) {
            serializer.elemBegin();
            auto inner = serializer.listBegin();
            for (auto v : axis) {
                serializer.elemBegin();
                serializer.putValue(v);
            }
            serializer.listEnd(inner);
        }
        serializer.listEnd(garr);
    }
}

::nicxlive::core::serde::SerdeException MeshData::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    vertices.clear();
    uvs.clear();
    indices.clear();
    gridAxes.clear();
    origin = Vec2{0, 0};

    if (auto verts = data.get_child_optional("verts")) {
        for (auto it = verts->begin(); it != verts->end();) {
            float x = it->second.get_value<float>();
            ++it;
            if (it == verts->end()) break;
            float y = it->second.get_value<float>();
            ++it;
            vertices.push_back(Vec2{x, y});
        }
    }

    if (auto u = data.get_child_optional("uvs")) {
        for (auto it = u->begin(); it != u->end();) {
            float x = it->second.get_value<float>();
            ++it;
            if (it == u->end()) break;
            float y = it->second.get_value<float>();
            ++it;
            uvs.push_back(Vec2{x, y});
        }
    }

    if (auto idx = data.get_child_optional("indices")) {
        for (const auto& i : *idx) {
            indices.push_back(i.second.get_value<uint16_t>());
        }
    }

    if (auto org = data.get_child_optional("origin")) {
        auto it = org->begin();
        if (it != org->end()) {
            origin.x = it->second.get_value<float>();
            ++it;
            if (it != org->end()) origin.y = it->second.get_value<float>();
        }
    }

    if (auto ga = data.get_child_optional("grid_axes")) {
        for (const auto& axis : *ga) {
            std::vector<float> vals;
            for (const auto& v : axis.second) {
                vals.push_back(v.second.get_value<float>());
            }
            gridAxes.push_back(std::move(vals));
        }
    }

    return std::nullopt;
}

void Drawable::updateBounds() {
    if (!gDoGenerateBounds) return;
    if (mesh->vertices.empty()) {
        bounds.reset();
        return;
    }
    Transform wtransform = transform();
    std::array<float, 4> b{
        wtransform.translation.x,
        wtransform.translation.y,
        wtransform.translation.x,
        wtransform.translation.y
    };
    Mat4 matrix = getDynamicMatrix();
    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
        Vec2 v = mesh->vertices[i];
        if (i < deformationOffsets.size()) {
            v.x += deformationOffsets[i].x;
            v.y += deformationOffsets[i].y;
        }
        if (i < deformation.size()) {
            v.x += deformation.xAt(i);
            v.y += deformation.yAt(i);
        }
        Vec3 oriented = matrix.transformPoint(Vec3{v.x, v.y, 0.0f});
        b[0] = std::min(b[0], oriented.x);
        b[1] = std::min(b[1], oriented.y);
        b[2] = std::max(b[2], oriented.x);
        b[3] = std::max(b[3], oriented.y);
    }
    bounds = b;
}

void Drawable::drawMeshLines() const {
    if (mesh->vertices.empty() || mesh->indices.empty()) return;
    Mat4 trans = getDynamicMatrix();
    if (auto ot = getOneTimeTransform()) trans = Mat4::multiply(*ot, trans);
    std::vector<Vec3> pts;
    pts.reserve(mesh->vertices.size());
    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
        pts.push_back(Vec3{mesh->vertices[i].x - mesh->origin.x + deformation[i].x, mesh->vertices[i].y - mesh->origin.y + deformation[i].y, 0});
    }
    std::vector<uint16_t> idx;
    idx.reserve(mesh->indices.size() * 2);
    for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
        auto i0 = mesh->indices[i];
        auto i1 = mesh->indices[i + 1];
        auto i2 = mesh->indices[i + 2];
        if (i0 >= pts.size() || i1 >= pts.size() || i2 >= pts.size()) continue;
        idx.push_back(static_cast<uint16_t>(i0));
        idx.push_back(static_cast<uint16_t>(i1));
        idx.push_back(static_cast<uint16_t>(i1));
        idx.push_back(static_cast<uint16_t>(i2));
        idx.push_back(static_cast<uint16_t>(i2));
        idx.push_back(static_cast<uint16_t>(i0));
    }
    if (auto backend = core::getCurrentRenderBackend()) {
        backend->setDebugLineWidth(1.0f);
        backend->uploadDebugBuffer(pts, idx);
        backend->drawDebugLines(Vec4{0.5f, 0.5f, 0.5f, 1.0f}, trans);
    }
}

void Drawable::drawMeshPoints() const {
    if (mesh->vertices.empty()) return;
    Mat4 trans = getDynamicMatrix();
    if (auto ot = getOneTimeTransform()) trans = Mat4::multiply(*ot, trans);
    std::vector<Vec3> pts;
    pts.reserve(mesh->vertices.size());
    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
        pts.push_back(Vec3{mesh->vertices[i].x - mesh->origin.x + deformation[i].x, mesh->vertices[i].y - mesh->origin.y + deformation[i].y, 0});
    }
    std::vector<uint16_t> idx;
    idx.reserve(mesh->vertices.size());
    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) idx.push_back(static_cast<uint16_t>(i));
    if (auto backend = core::getCurrentRenderBackend()) {
        backend->setDebugPointSize(4.0f);
        backend->uploadDebugBuffer(pts, idx);
        backend->drawDebugPoints(Vec4{1, 1, 1, 1}, trans);
    }
}
MeshData& Drawable::getMesh() { return *mesh; }

void Drawable::updateIndices() {
    auto backend = core::getCurrentRenderBackend();
    if (!backend) return;
    if (mesh->indices.empty()) return;
    backend->initializeDrawableResources();
    backend->bindDrawableVao();
    if (ibo == 0) {
        backend->createDrawableBuffers(ibo);
    }
    backend->uploadDrawableIndices(ibo, mesh->indices);
}

std::tuple<Vec2Array, std::optional<Mat4>, bool> Drawable::nodeAttachProcessor(const std::shared_ptr<Node>& node,
                                                                               const Vec2Array& origVertices,
                                                                               const Vec2Array& origDeformation,
                                                                               const Mat4* origTransform) {
    bool changed = false;
    if (!node || mesh->indices.size() < 3) return {origDeformation, std::nullopt, false};
    Mat4 inv = Mat4::inverse(transform().toMat4());
    Vec3 localNode = inv.transformPoint(Vec3{node->transform().translation.x, node->transform().translation.y, 0});
    Vec2 nodeOrigin{localNode.x, localNode.y};

    auto triIt = attachedIndex.find(node->uuid);
    std::array<std::size_t, 3> tri{};
    if (triIt == attachedIndex.end()) {
        auto triFound = findSurroundingTriangle(nodeOrigin, static_cast<const MeshData&>(*mesh));
        if (!triFound) return {origDeformation, std::nullopt, false};
        tri = *triFound;
        attachedIndex[node->uuid] = tri;
    } else {
        tri = triIt->second;
    }

    Vec2 targetPrime{};
    float rotVert = 0.0f, rotHorz = 0.0f;
    if (!calculateTransformInTriangle(*mesh, tri, deformation, nodeOrigin, targetPrime, rotVert, rotHorz)) {
        return {origDeformation, std::nullopt, false};
    }
    Vec2 delta{targetPrime.x - nodeOrigin.x, targetPrime.y - nodeOrigin.y};
    node->setValue("transform.t.x", delta.x);
    node->setValue("transform.t.y", delta.y);
    node->setValue("transform.r.z", (rotVert + rotHorz) / 2.0f);
    node->transformChanged();
    changed = true;
    return {Vec2Array{}, std::nullopt, changed};
}

void Drawable::updateVertices() {
    sharedVertexResize(vertices, mesh->vertices.size());
    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
        vertices.set(i, mesh->vertices[i]);
    }
    sharedVertexMarkDirty();

    deformation.resize(vertices.size());
    deformation.fill(Vec2{0.0f, 0.0f});
    deformationOffsets.resize(vertices.size(), Vec2{});
    sharedDeformResize(deformation, deformation.size());
    sharedDeformMarkDirty();
    updateDeform();
}

void Drawable::rebufferMesh(const MeshData& data) {
    sharedUvResize(sharedUvs, data.uvs.size());
    for (std::size_t i = 0; i < data.uvs.size(); ++i) {
        sharedUvs.set(i, data.uvs[i]);
    }
    sharedUvMarkDirty();
    *mesh = data;
    updateIndices();
    updateVertices();
}

void Drawable::updateDeform() {
    Deformable::updateDeform();
    sharedDeformMarkDirty();
    updateBounds();
}

void Drawable::reset() {
    sharedVertexResize(vertices, mesh->vertices.size());
    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
        vertices.set(i, mesh->vertices[i]);
    }
    sharedVertexMarkDirty();
}

bool Drawable::isWeldedBy(NodeId target) const {
    return std::any_of(weldedLinks.begin(), weldedLinks.end(), [&](const WeldingLink& l) { return l.targetUUID == target; });
}

std::tuple<Vec2Array, std::optional<Mat4>, bool> Drawable::weldingProcessor(const std::shared_ptr<Node>& target,
                                                                            const Vec2Array& origVertices,
                                                                            const Vec2Array& origDeformation,
                                                                            const Mat4* origTransform) {
    auto targetDrawable = std::dynamic_pointer_cast<Drawable>(target);
    if (!targetDrawable) return {origDeformation, std::nullopt, false};
    auto it = std::find_if(weldedLinks.begin(), weldedLinks.end(), [&](const WeldingLink& l) { return l.targetUUID == targetDrawable->uuid; });
    if (it == weldedLinks.end()) return {origDeformation, std::nullopt, false};
    if (postProcessed < 2) return {Vec2Array{}, std::nullopt, false};
    if (weldingApplied.count(targetDrawable->uuid) || targetDrawable->weldingApplied.count(uuid)) {
        return {Vec2Array{}, std::nullopt, false};
    }
    weldingApplied.insert(targetDrawable->uuid);
    targetDrawable->weldingApplied.insert(uuid);

    const auto& link = *it;
    bool changed = false;
    auto pairCount = std::min<std::size_t>(link.indices.size(), vertices.size());
    if (pairCount == 0) return {origDeformation, std::nullopt, false};

    std::vector<std::size_t> selfIndices;
    std::vector<std::size_t> targetIndices;
    selfIndices.reserve(pairCount);
    targetIndices.reserve(pairCount);
    for (std::size_t i = 0; i < pairCount; ++i) {
        auto mapped = link.indices[i];
        if (mapped == NOINDEX || mapped < 0) continue;
        auto tIdx = static_cast<std::size_t>(mapped);
        if (tIdx >= origVertices.size()) continue;
        selfIndices.push_back(i);
        targetIndices.push_back(tIdx);
    }
    if (selfIndices.empty()) return {origDeformation, std::nullopt, false};

    Vec2Array selfLocal = gatherVec2(vertices, selfIndices);
    Vec2Array selfDelta = gatherVec2(deformation, selfIndices);
    selfLocal += selfDelta;

    Vec2Array targetLocal = gatherVec2(origVertices, targetIndices);
    Vec2Array targetDelta = gatherVec2(origDeformation, targetIndices);
    targetLocal += targetDelta;

    Mat4 selfMatrix = overrideTransformMatrix ? *overrideTransformMatrix : transform().toMat4();
    Mat4 targetMatrix = origTransform ? *origTransform : targetDrawable->transform().toMat4();

    Vec2Array selfWorld;
    transformAssign(selfWorld, selfLocal, selfMatrix);
    Vec2Array targetWorld;
    transformAssign(targetWorld, targetLocal, targetMatrix);

    float weldingWeight = std::clamp(link.weight, 0.0f, 1.0f);
    Vec2Array blended = targetWorld;
    blended *= (1.0f - weldingWeight);
    Vec2Array weightedSelf = selfWorld;
    weightedSelf *= weldingWeight;
    blended += weightedSelf;

    Vec2Array deltaSelf = blended;
    deltaSelf -= selfWorld;
    Vec2Array deltaTarget = blended;
    deltaTarget -= targetWorld;

    Mat4 selfInv = Mat4::inverse(selfMatrix);
    selfInv[0][3] = selfInv[1][3] = selfInv[2][3] = 0.0f;
    Mat4 targetInv = Mat4::inverse(targetMatrix);
    targetInv[0][3] = targetInv[1][3] = targetInv[2][3] = 0.0f;

    Vec2Array localSelf = common::makeZeroVecArray(selfIndices.size());
    transformAdd(localSelf, deltaSelf, selfInv);
    Vec2Array localTarget = common::makeZeroVecArray(targetIndices.size());
    transformAdd(localTarget, deltaTarget, targetInv);

    scatterAddVec2(localSelf, selfIndices, deformation, changed);
    Vec2Array targetDeformOut = origDeformation;
    scatterAddVec2(localTarget, targetIndices, targetDeformOut, changed);
    sharedDeformMarkDirty();
    return {targetDeformOut, std::nullopt, changed};
}

void Drawable::addWeldedTarget(const std::shared_ptr<Drawable>& target,
                               const std::vector<std::ptrdiff_t>& indices,
                               float weight) {
    if (!target) return;
    auto existing = std::find_if(weldedLinks.begin(), weldedLinks.end(), [&](const WeldingLink& l) { return l.targetUUID == target->uuid; });
    if (existing == weldedLinks.end()) {
        weldedLinks.push_back(WeldingLink{target->uuid, target, indices, weight});
    } else {
        existing->indices = indices;
        existing->weight = weight;
    }
    auto reciprocal = std::find_if(target->weldedLinks.begin(), target->weldedLinks.end(), [&](const WeldingLink& l) { return l.targetUUID == uuid; });
    if (reciprocal == target->weldedLinks.end()) {
        std::vector<std::ptrdiff_t> counter(target->mesh->vertices.size(), NOINDEX);
        for (std::size_t i = 0; i < indices.size() && i < counter.size(); ++i) {
            auto ind = indices[i];
            if (ind != NOINDEX && ind >= 0) counter[static_cast<std::size_t>(ind)] = static_cast<std::ptrdiff_t>(i);
        }
        target->weldedLinks.push_back(WeldingLink{uuid, std::dynamic_pointer_cast<Drawable>(shared_from_this()), counter, 1.0f - weight});
    }
    if (!isWeldedBy(target->uuid)) weldedTargets.push_back(target->uuid);
    if (!target->isWeldedBy(uuid)) target->weldedTargets.push_back(uuid);
    welded = target->welded = true;
    registerWeldFilter(target);
    target->registerWeldFilter(std::dynamic_pointer_cast<Drawable>(shared_from_this()));
}

void Drawable::removeWeldedTarget(const std::shared_ptr<Drawable>& target) {
    if (!target) return;
    weldedLinks.erase(std::remove_if(weldedLinks.begin(), weldedLinks.end(), [&](const WeldingLink& l) { return l.targetUUID == target->uuid; }), weldedLinks.end());
    weldedTargets.erase(std::remove(weldedTargets.begin(), weldedTargets.end(), target->uuid), weldedTargets.end());
    target->weldedLinks.erase(std::remove_if(target->weldedLinks.begin(), target->weldedLinks.end(), [&](const WeldingLink& l) { return l.targetUUID == uuid; }), target->weldedLinks.end());
    target->weldedTargets.erase(std::remove(target->weldedTargets.begin(), target->weldedTargets.end(), uuid), target->weldedTargets.end());
    unregisterWeldFilter(target);
    target->unregisterWeldFilter(std::dynamic_pointer_cast<Drawable>(shared_from_this()));
}

void Drawable::setupSelf() {
    Deformable::setupSelf();
    for (auto& link : weldedLinks) {
        auto pup = puppetRef();
        if (!pup) continue;
        std::shared_ptr<Drawable> tgt;
        if (auto locked = link.target.lock()) {
            tgt = locked;
        } else {
            tgt = std::dynamic_pointer_cast<Drawable>(pup->findNodeById(link.targetUUID));
            link.target = tgt;
        }
        registerWeldFilter(tgt);
    }
}

void Drawable::finalizeDrawable() {
    for (auto& child : children) {
        if (child && child->pinToMesh) {
            setupChild(child);
        }
    }
    std::vector<WeldingLink> valid;
    if (auto pup = puppetRef()) {
        for (auto& link : weldedLinks) {
            std::shared_ptr<Drawable> tgt;
            if (auto locked = link.target.lock()) {
                tgt = locked;
            } else {
                tgt = std::dynamic_pointer_cast<Drawable>(pup->findNodeById(link.targetUUID));
                link.target = tgt;
            }
            if (tgt) {
                valid.push_back(link);
                registerWeldFilter(tgt);
            }
        }
    }
    weldedLinks = valid;
    buildDrawable(true);
}

void Drawable::clearCache() {
    deformationOffsets.clear();
    bounds.reset();
    weldingApplied.clear();
    attachedIndex.clear();
    sharedVertexMarkDirty();
    sharedUvMarkDirty();
    sharedDeformMarkDirty();
}

void Drawable::runBeginTask(core::RenderContext& ctx) {
    weldingApplied.clear();
    Deformable::runBeginTask(ctx);
}

void Drawable::runPostTaskImpl(std::size_t priority, core::RenderContext& ctx) {
    Deformable::runPostTaskImpl(priority, ctx);
}

void Drawable::normalizeUv() {
    if (mesh->uvs.empty()) return;
    float minX = mesh->uvs[0].x;
    float maxX = minX;
    float minY = mesh->uvs[0].y;
    float maxY = minY;
    for (std::size_t i = 1; i < mesh->uvs.size(); ++i) {
        const auto& uv = mesh->uvs[i];
        minX = std::min(minX, uv.x);
        maxX = std::max(maxX, uv.x);
        minY = std::min(minY, uv.y);
        maxY = std::max(maxY, uv.y);
    }
    float width = maxX - minX;
    float height = maxY - minY;
    float centerX = width != 0.0f ? (minX + maxX) / (2.0f * width) : 0.0f;
    float centerY = height != 0.0f ? (minY + maxY) / (2.0f * height) : 0.0f;
    for (auto& uv : mesh->uvs) {
        if (width != 0.0f) uv.x /= width;
        if (height != 0.0f) uv.y /= height;
        uv.x += (0.5f - centerX);
        uv.y += (0.5f - centerY);
    }
    sharedUvMarkDirty();
}

void Drawable::centralizeDrawable() {
    for (auto& c : children) {
        if (c) {
            auto drawableChild = std::dynamic_pointer_cast<Drawable>(c);
            if (drawableChild) drawableChild->centralizeDrawable();
        }
    }
    updateBounds();
}

void Drawable::copyFromDrawable(const Drawable& src) {
    mesh->vertices = src.mesh->vertices;
    mesh->uvs = src.mesh->uvs.empty() ? src.mesh->vertices : src.mesh->uvs;
    mesh->indices = src.mesh->indices;
    mesh->origin = src.mesh->origin;
    mesh->gridAxes = src.mesh->gridAxes;
    deformation = src.deformation;
    tint = src.tint;
    screenTint = src.screenTint;
    emissionStrength = src.emissionStrength;
    textures = src.textures;
    bounds = src.bounds;
    weldedTargets = src.weldedTargets;
    weldedLinks = src.weldedLinks;
    weldingApplied = src.weldingApplied;
    vertexOffset = uvOffset = deformOffset = 0;
    sharedUvResize(sharedUvs, mesh->uvs.size());
    for (std::size_t i = 0; i < mesh->uvs.size(); ++i) {
        sharedUvs.set(i, mesh->uvs[i]);
    }
    sharedUvMarkDirty();
    updateVertices();
    updateIndices();
}

void Drawable::setupChildDrawable() {
    for (auto& child : children) {
        if (!child) continue;
        if (!child->pinToMesh) continue;
        bool exists = std::any_of(child->preProcessFilters.begin(), child->preProcessFilters.end(), [](const FilterHook& h) {
            return h.stage == 0 && h.tag == kNodeAttachFilterTag;
        });
        if (exists) continue;
        std::weak_ptr<Drawable> weakSelf = std::dynamic_pointer_cast<Drawable>(shared_from_this());
        FilterHook hook;
        hook.stage = 0;
        hook.tag = kNodeAttachFilterTag;
        hook.func = [weakSelf](std::shared_ptr<Node> self, const std::vector<Vec2>& verts, const std::vector<Vec2>& deform, const Mat4* mat) {
            auto owner = weakSelf.lock();
            if (!owner) return std::make_tuple(std::vector<Vec2>{}, std::optional<Mat4>{}, false);
            Vec2Array v;
            v.resize(verts.size());
            for (std::size_t i = 0; i < verts.size(); ++i) {
                v.set(i, verts[i]);
            }
            Vec2Array d;
            d.resize(deform.size());
            for (std::size_t i = 0; i < deform.size(); ++i) {
                d.set(i, deform[i]);
            }
            auto res = owner->nodeAttachProcessor(self, v, d, mat);
            std::vector<Vec2> out;
            const auto& def = std::get<0>(res);
            out.reserve(def.size());
            for (std::size_t i = 0; i < def.size(); ++i) {
                out.push_back(Vec2{def.xAt(i), def.yAt(i)});
            }
            return std::make_tuple(out, std::optional<Mat4>{}, std::get<2>(res));
        };
        child->preProcessFilters.push_back(std::move(hook));
    }
}

void Drawable::releaseChildDrawable() {
    for (auto& child : children) {
        if (!child) continue;
        child->preProcessFilters.erase(std::remove_if(child->preProcessFilters.begin(), child->preProcessFilters.end(), [](const FilterHook& h) {
            return h.stage == 0 && h.tag == kNodeAttachFilterTag;
        }), child->preProcessFilters.end());
        attachedIndex.erase(child->uuid);
    }
}

void Drawable::build(bool force) {
    (void)force;
    setupChildDrawable();
    setupSelf();
    Node::build(force);
}

void Drawable::buildDrawable(bool force) {
    auto backend = core::getCurrentRenderBackend();
    if (force || !mesh->isReady()) {
        if (backend) {
            backend->initializeDrawableResources();
            backend->bindDrawableVao();
        }
        updateIndices();
        updateVertices();
    }
    if (backend && ibo != 0 && !mesh->indices.empty()) {
        backend->drawDrawableElements(ibo, mesh->indices.size());
    }
}

bool Drawable::mustPropagateDrawable() const { return true; }

void Drawable::fillDrawPacket(const Node& header, PartDrawPacket& packet, bool /*isMask*/) const {
    packet.renderable = header.enabled && mesh->isReady();
    packet.modelMatrix = header.transform().toMat4();
    if (auto pup = puppetRef()) {
        packet.puppetMatrix = pup->transform.toMat4();
    }
    packet.ignorePuppet = false;
    packet.opacity = 1.0f;
    packet.emissionStrength = emissionStrength;
    packet.blendMode = BlendMode::Normal;
    packet.useMultistageBlend = useMultistageBlend(packet.blendMode);
    packet.hasEmissionOrBumpmap = (textures.size() > 1 && textures[1]) || (textures.size() > 2 && textures[2]);
    packet.maskThreshold = 0.0f;
    packet.clampedTint = tint;
    packet.clampedScreen = screenTint;
    packet.origin = mesh->origin;
    packet.vertexOffset = vertexOffset;
    packet.uvOffset = uvOffset;
    packet.deformOffset = deformOffset;
    packet.vertexAtlasStride = sharedVertexBufferData().size();
    packet.uvAtlasStride = sharedUvBufferData().size();
    packet.deformAtlasStride = sharedDeformBufferData().size();
    packet.vertexCount = static_cast<uint32_t>(mesh->vertices.size());
    packet.indexCount = static_cast<uint32_t>(mesh->indices.size());
    for (std::size_t i = 0; i < textures.size() && i < 3; ++i) {
        if (textures[i]) {
            uint32_t texId = textures[i]->getRuntimeUUID();
            if (texId == 0) texId = textures[i]->backendId();
            packet.textureUUIDs[i] = texId;
        } else {
            packet.textureUUIDs[i] = 0;
        }
    }
    packet.vertices = mesh->vertices;
    packet.uvs = mesh->uvs;
    packet.indices = mesh->indices;
    packet.deformation = deformation;
    packet.node.reset();
}

void Drawable::registerWeldFilter(const std::shared_ptr<Drawable>& target) {
    if (!target) return;
    const auto tag = kWeldFilterTagBase | static_cast<std::uintptr_t>(target->uuid);
    // D parity: welding filter registration is upsert, not append.
    auto exists = std::any_of(postProcessFilters.begin(), postProcessFilters.end(), [&](const FilterHook& h) {
        return h.stage == 2 && h.tag == tag;
    });
    if (exists) return;
    FilterHook hook;
    hook.stage = 2;
    hook.tag = tag;
    std::weak_ptr<Drawable> weakTarget = target;
    hook.func = [weakTarget](std::shared_ptr<Node> self, const std::vector<Vec2>& verts, const std::vector<Vec2>& deform, const Mat4* mat) {
        auto tgt = weakTarget.lock();
        if (!tgt) return std::make_tuple(std::vector<Vec2>{}, std::optional<Mat4>{}, false);
        Vec2Array v; v.resize(verts.size());
        for (std::size_t i = 0; i < verts.size(); ++i) { v.set(i, verts[i]); }
        Vec2Array d; d.resize(deform.size());
        for (std::size_t i = 0; i < deform.size(); ++i) { d.set(i, deform[i]); }
        auto res = tgt->weldingProcessor(self, v, d, mat);
        std::vector<Vec2> out;
        const auto& def = std::get<0>(res);
        out.reserve(def.size());
        for (std::size_t i = 0; i < def.size(); ++i) out.push_back(Vec2{def.xAt(i), def.yAt(i)});
        return std::make_tuple(out, std::optional<Mat4>{}, std::get<2>(res));
    };
    postProcessFilters.push_back(std::move(hook));
}

void Drawable::unregisterWeldFilter(const std::shared_ptr<Drawable>& target) {
    if (!target) return;
    const auto tag = kWeldFilterTagBase | static_cast<std::uintptr_t>(target->uuid);
    postProcessFilters.erase(std::remove_if(postProcessFilters.begin(), postProcessFilters.end(), [&](const FilterHook& h) {
        return h.stage == 2 && h.tag == tag;
    }), postProcessFilters.end());
}

void Drawable::serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const {
    Node::serializeSelfImpl(serializer, recursive, flags);
    if (has_flag(flags, SerializeNodeFlags::Geometry)) {
        serializer.putKey("mesh");
        mesh->serialize(serializer);
    }
    if (has_flag(flags, SerializeNodeFlags::Links) && !weldedLinks.empty()) {
        serializer.putKey("weldedLinks");
        auto arr = serializer.listBegin();
        for (const auto& link : weldedLinks) {
            serializer.elemBegin();
            serializer.putKey("targetUUID");
            serializer.putValue(link.targetUUID);
            serializer.putKey("weight");
            serializer.putValue(link.weight);
            serializer.putKey("indices");
            auto idxArr = serializer.listBegin();
            for (auto idx : link.indices) {
                serializer.elemBegin();
                serializer.putValue(static_cast<std::ptrdiff_t>(idx));
            }
            serializer.listEnd(idxArr);
        }
        serializer.listEnd(arr);
    }
}

::nicxlive::core::serde::SerdeException Drawable::deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) {
    if (auto err = Node::deserializeFromFghj(data)) return err;
    if (auto m = data.get_child_optional("mesh")) {
        if (auto exc = mesh->deserializeFromFghj(*m)) return exc;
        vertices.resize(mesh->vertices.size());
        for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
            vertices.set(i, mesh->vertices[i]);
        }
        deformation.resize(mesh->vertices.size());
        updateIndices();
        updateVertices();
    }
    if (auto wl = data.get_child_optional("weldedLinks")) {
        weldedLinks.clear();
        for (const auto& child : *wl) {
            WeldingLink link;
            link.targetUUID = child.second.get<uint32_t>("targetUUID", 0);
            link.weight = child.second.get<float>("weight", 0.0f);
            if (auto idx = child.second.get_child_optional("indices")) {
                for (const auto& i : *idx) {
                    link.indices.push_back(i.second.get_value<std::ptrdiff_t>());
                }
            }
            weldedLinks.push_back(std::move(link));
        }
    }
    return std::nullopt;
}

} // namespace nicxlive::core::nodes
