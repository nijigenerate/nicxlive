#include "drawable.hpp"

#include "node.hpp"
#include "../puppet.hpp"

#include "../serde.hpp"

#include <algorithm>
#include <map>

namespace nicxlive::core::nodes {

namespace {
Vec2Array gSharedVertices;
Vec2Array gSharedUvs;
Vec2Array gSharedDeform;
std::map<NodeId, std::size_t> gVertexOffsets;
std::map<NodeId, std::size_t> gUvOffsets;
std::map<NodeId, std::size_t> gDeformOffsets;
bool gSharedVerticesDirty = false;
bool gSharedUvsDirty = false;
bool gSharedDeformDirty = false;

void ensureCapacity(Vec2Array& buf, std::size_t offset, std::size_t count) {
    if (buf.size() < offset + count) {
        buf.resize(offset + count);
    }
}

void writeBuffer(const Vec2Array& src, Vec2Array& dst, std::size_t offset) {
    ensureCapacity(dst, offset, src.size());
    for (std::size_t i = 0; i < src.size(); ++i) {
        dst.x[offset + i] = src.x[i];
        dst.y[offset + i] = src.y[i];
    }
}
} // namespace

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

Vec2Array& sharedVertexBufferData() { return gSharedVertices; }
Vec2Array& sharedUvBufferData() { return gSharedUvs; }
Vec2Array& sharedDeformBufferData() { return gSharedDeform; }

void sharedVertexResize(Vec2Array& target, std::size_t newLength) { target.resize(newLength); gSharedVerticesDirty = true; }
void sharedUvResize(Vec2Array& target, std::size_t newLength) { target.resize(newLength); gSharedUvsDirty = true; }
void sharedUvResize(std::vector<Vec2>& target, std::size_t newLength) { target.resize(newLength); gSharedUvsDirty = true; }
void sharedDeformResize(Vec2Array& target, std::size_t newLength) { target.resize(newLength); gSharedDeformDirty = true; }
void sharedVertexMarkDirty() { gSharedVerticesDirty = true; }
void sharedUvMarkDirty() { gSharedUvsDirty = true; }
void sharedDeformMarkDirty() { gSharedDeformDirty = true; }

void Drawable::updateBounds() {
    if (mesh.vertices.empty()) {
        bounds.reset();
        return;
    }
    float minx = mesh.vertices[0].x, maxx = mesh.vertices[0].x;
    float miny = mesh.vertices[0].y, maxy = mesh.vertices[0].y;
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        auto vx = mesh.vertices[i].x;
        auto vy = mesh.vertices[i].y;
        if (i < deformationOffsets.size()) {
            vx += deformationOffsets[i].x;
            vy += deformationOffsets[i].y;
        }
        if (i < deformation.x.size()) {
            vx += deformation.x[i];
            vy += deformation.y[i];
        }
        minx = std::min(minx, vx);
        maxx = std::max(maxx, vx);
        miny = std::min(miny, vy);
        maxy = std::max(maxy, vy);
    }
    bounds = std::array<float, 4>{minx, miny, maxx, maxy};
}

void Drawable::drawMeshLines() const {}
void Drawable::drawMeshPoints() const {}
void Drawable::getMesh() {}

void Drawable::updateIndices() {
    // Validate indices; in absence of backend, ensure consistency
    if (mesh.indices.empty() || (mesh.indices.size() % 3) != 0) {
        mesh.indices.clear();
        return;
    }
    uint16_t maxIdx = 0;
    for (auto v : mesh.indices) {
        if (v > maxIdx) maxIdx = v;
    }
    if (maxIdx >= mesh.vertices.size()) {
        mesh.indices.clear();
        return;
    }
}

static std::size_t registerBuffer(std::map<NodeId, std::size_t>& table, NodeId id, const Vec2Array& data, Vec2Array& buf) {
    auto it = table.find(id);
    if (it != table.end()) return it->second;
    std::size_t offset = buf.size();
    ensureCapacity(buf, offset, data.size());
    table[id] = offset;
    return offset;
}

std::tuple<Vec2Array, std::optional<Mat4>, bool> Drawable::nodeAttachProcessor(const std::shared_ptr<Node>& node,
                                                                               const Vec2Array& origVertices,
                                                                               const Vec2Array& origDeformation,
                                                                               const Mat4* origTransform) {
    bool changed = false;
    if (!node || origVertices.size() == 0 || mesh.indices.size() < 3) return {origDeformation, std::nullopt, false};
    Mat4 inv = origTransform ? Mat4::inverse(*origTransform) : Mat4::identity();
    Vec3 localNode = inv.transformPoint(Vec3{node->transform().translation.x, node->transform().translation.y, 0});
    // iterate triangles
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        auto i0 = mesh.indices[i];
        auto i1 = mesh.indices[i + 1];
        auto i2 = mesh.indices[i + 2];
        if (i0 >= origVertices.size() || i1 >= origVertices.size() || i2 >= origVertices.size()) continue;
        Vec2 v0{origVertices.x[i0], origVertices.y[i0]};
        Vec2 v1{origVertices.x[i1], origVertices.y[i1]};
        Vec2 v2{origVertices.x[i2], origVertices.y[i2]};
        float denom = (v1.y - v2.y) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.y - v2.y);
        if (denom == 0.0f) continue;
        float a = ((v1.y - v2.y) * (localNode.x - v2.x) + (v2.x - v1.x) * (localNode.y - v2.y)) / denom;
        float b = ((v2.y - v0.y) * (localNode.x - v2.x) + (v0.x - v2.x) * (localNode.y - v2.y)) / denom;
        float c = 1.0f - a - b;
        if (a < 0 || b < 0 || c < 0) continue;
        node->setValue("transform.t.x", localNode.x);
        node->setValue("transform.t.y", localNode.y);
        changed = true;
        return {origDeformation, std::nullopt, changed};
    }
    return {origDeformation, std::nullopt, changed};
}

void Drawable::writeSharedBuffers() {
    if (!mesh.vertices.empty()) {
        if (vertexOffset == 0 && uuid != 0 && gVertexOffsets.count(uuid) == 0) {
            vertexOffset = static_cast<uint32_t>(registerBuffer(gVertexOffsets, uuid, vertices, gSharedVertices));
        }
        writeBuffer(vertices, gSharedVertices, vertexOffset);
        gSharedVerticesDirty = true;
    }
    if (!mesh.uvs.empty()) {
        if (uvOffset == 0 && uuid != 0 && gUvOffsets.count(uuid) == 0) {
            uvOffset = static_cast<uint32_t>(registerBuffer(gUvOffsets, uuid, vertices, gSharedUvs));
        }
        Vec2Array uva(mesh.uvs.size());
        for (std::size_t i = 0; i < mesh.uvs.size(); ++i) { uva.x[i] = mesh.uvs[i].x; uva.y[i] = mesh.uvs[i].y; }
        writeBuffer(uva, gSharedUvs, uvOffset);
        gSharedUvsDirty = true;
    }
    if (deformation.size() > 0) {
        if (deformOffset == 0 && uuid != 0 && gDeformOffsets.count(uuid) == 0) {
            deformOffset = static_cast<uint32_t>(registerBuffer(gDeformOffsets, uuid, deformation, gSharedDeform));
        }
        writeBuffer(deformation, gSharedDeform, deformOffset);
        gSharedDeformDirty = true;
    }
}

void Drawable::updateVertices() {
    Vec2Array verts(mesh.vertices.size());
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        verts.x[i] = mesh.vertices[i].x;
        verts.y[i] = mesh.vertices[i].y;
    }
    vertices = verts;
    deformation.resize(vertices.size());
    std::fill(deformation.x.begin(), deformation.x.end(), 0.0f);
    std::fill(deformation.y.begin(), deformation.y.end(), 0.0f);
    deformationOffsets.resize(vertices.size(), Vec2{});
    updateBounds();
    writeSharedBuffers();
}

void Drawable::rebufferMesh() { updateVertices(); updateIndices(); }

void Drawable::reset() {
    mesh = MeshData{};
    deformationOffsets.clear();
    weldedTargets.clear();
    weldedLinks.clear();
    weldingApplied.clear();
    bounds.reset();
}

bool Drawable::isWeldedBy(NodeId target) const {
    return std::find(weldedTargets.begin(), weldedTargets.end(), target) != weldedTargets.end();
}

std::tuple<Vec2Array, std::optional<Mat4>, bool> Drawable::weldingProcessor(const std::shared_ptr<Node>& target,
                                                                            const Vec2Array& origVertices,
                                                                            const Vec2Array& origDeformation,
                                                                            const Mat4* origTransform) {
    auto targetDrawable = std::dynamic_pointer_cast<Drawable>(target);
    if (!targetDrawable) return {origDeformation, std::nullopt, false};
    auto it = std::find_if(weldedLinks.begin(), weldedLinks.end(), [&](const WeldingLink& l) { return l.target == targetDrawable->uuid; });
    if (it == weldedLinks.end()) return {origDeformation, std::nullopt, false};
    if (postProcessed < 2) return {Vec2Array{}, std::nullopt, false};
    if (weldingApplied.count(targetDrawable->uuid) || targetDrawable->weldingApplied.count(uuid)) {
        return {Vec2Array{}, std::nullopt, false};
    }
    weldingApplied.insert(targetDrawable->uuid);
    targetDrawable->weldingApplied.insert(uuid);

    const auto& link = *it;
    bool changed = false;
    auto pairCount = std::min<std::size_t>(link.indices.size(), origVertices.size());
    if (pairCount == 0) return {origDeformation, std::nullopt, false};

    std::vector<std::size_t> selfIndices;
    std::vector<std::size_t> targetIndices;
    selfIndices.reserve(pairCount);
    targetIndices.reserve(pairCount);
    for (std::size_t i = 0; i < pairCount; ++i) {
        auto mapped = link.indices[i];
        if (mapped == NOINDEX || mapped < 0) continue;
        auto tIdx = static_cast<std::size_t>(mapped);
        if (tIdx >= targetDrawable->vertices.size()) continue;
        selfIndices.push_back(i);
        targetIndices.push_back(tIdx);
    }
    if (selfIndices.empty()) return {origDeformation, std::nullopt, false};

    Vec2Array selfLocal = gatherVec2(vertices, selfIndices);
    Vec2Array selfDelta = gatherVec2(deformation, selfIndices);
    selfLocal += selfDelta;

    Vec2Array targetLocal = gatherVec2(targetDrawable->vertices, targetIndices);
    Vec2Array targetDelta = gatherVec2(targetDrawable->deformation, targetIndices);
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

    Vec2Array deformationOut = origDeformation;
    scatterAddVec2(localSelf, selfIndices, deformationOut, changed);
    scatterAddVec2(localTarget, targetIndices, targetDrawable->deformation, changed);

    return {deformationOut, std::nullopt, changed};
}

void Drawable::addWeldedTarget(const std::shared_ptr<Drawable>& target,
                               const std::vector<std::ptrdiff_t>& indices,
                               float weight) {
    if (!target) return;
    auto existing = std::find_if(weldedLinks.begin(), weldedLinks.end(), [&](const WeldingLink& l) { return l.target == target->uuid; });
    if (existing == weldedLinks.end()) {
        weldedLinks.push_back(WeldingLink{target->uuid, indices, weight});
    } else {
        existing->indices = indices;
        existing->weight = weight;
    }
    auto reciprocal = std::find_if(target->weldedLinks.begin(), target->weldedLinks.end(), [&](const WeldingLink& l) { return l.target == uuid; });
    if (reciprocal == target->weldedLinks.end()) {
        target->weldedLinks.push_back(WeldingLink{uuid, std::vector<std::ptrdiff_t>(target->mesh.vertices.size(), NOINDEX), 1.0f - weight});
    }
    weldedTargets.push_back(target->uuid);
    target->weldedTargets.push_back(uuid);
    welded = target->welded = true;
    registerWeldFilter(target);
    target->registerWeldFilter(std::dynamic_pointer_cast<Drawable>(shared_from_this()));
}

void Drawable::removeWeldedTarget(const std::shared_ptr<Drawable>& target) {
    if (!target) return;
    weldedLinks.erase(std::remove_if(weldedLinks.begin(), weldedLinks.end(), [&](const WeldingLink& l) { return l.target == target->uuid; }), weldedLinks.end());
    weldedTargets.erase(std::remove(weldedTargets.begin(), weldedTargets.end(), target->uuid), weldedTargets.end());
    target->weldedLinks.erase(std::remove_if(target->weldedLinks.begin(), target->weldedLinks.end(), [&](const WeldingLink& l) { return l.target == uuid; }), target->weldedLinks.end());
    target->weldedTargets.erase(std::remove(target->weldedTargets.begin(), target->weldedTargets.end(), uuid), target->weldedTargets.end());
    unregisterWeldFilter(target);
    target->unregisterWeldFilter(std::dynamic_pointer_cast<Drawable>(shared_from_this()));
}

void Drawable::setupSelf() {
    Deformable::setupSelf();
    updateVertices();
    // ensure weld hooks registered
    for (auto& link : weldedLinks) {
        auto pup = puppetRef();
        if (!pup) continue;
        auto tgt = std::dynamic_pointer_cast<Drawable>(pup->findNodeById(link.target));
        registerWeldFilter(tgt);
    }
}

void Drawable::finalizeDrawable() {
    updateIndices();
    updateVertices();
    writeSharedBuffers();
}

void Drawable::clearCache() {
    deformationOffsets.clear();
    bounds.reset();
}

void Drawable::runBeginTask(core::RenderContext& ctx) {
    weldingApplied.clear();
    Deformable::runBeginTask(ctx);
    writeSharedBuffers();
    gSharedVerticesDirty = gSharedUvsDirty = gSharedDeformDirty = false;
}

void Drawable::runPostTaskImpl(std::size_t priority, core::RenderContext& ctx) {
    Deformable::runPostTaskImpl(priority, ctx);
    if (priority == 2) {
        auto pup = puppetRef();
        for (const auto& link : weldedLinks) {
            std::shared_ptr<Node> tgtNode;
            if (pup) tgtNode = pup->findNodeById(link.target);
            auto tgtDrawable = std::dynamic_pointer_cast<Drawable>(tgtNode);
            if (!tgtDrawable) continue;
            Mat4 tmat = tgtDrawable->transform().toMat4();
            auto res = weldingProcessor(tgtDrawable, vertices, deformation, &tmat);
            if (std::get<0>(res).size() == deformation.size()) {
                deformation = std::get<0>(res);
            }
            if (std::get<2>(res)) {
                changed = true;
            }
        }
    }
    writeSharedBuffers();
}

void Drawable::normalizeUv() {
    if (mesh.uvs.empty()) return;
    float minx = mesh.uvs[0].x, maxx = mesh.uvs[0].x;
    float miny = mesh.uvs[0].y, maxy = mesh.uvs[0].y;
    for (const auto& uv : mesh.uvs) {
        minx = std::min(minx, uv.x);
        maxx = std::max(maxx, uv.x);
        miny = std::min(miny, uv.y);
        maxy = std::max(maxy, uv.y);
    }
    float dx = maxx - minx;
    float dy = maxy - miny;
    for (auto& uv : mesh.uvs) {
        uv.x = dx != 0.0f ? (uv.x - minx) / dx : 0.0f;
        uv.y = dy != 0.0f ? (uv.y - miny) / dy : 0.0f;
    }
}

void Drawable::centralizeDrawable() {
    if (mesh.vertices.empty()) return;
    Vec2 center{0, 0};
    for (auto& v : mesh.vertices) {
        center.x += v.x;
        center.y += v.y;
    }
    center.x /= static_cast<float>(mesh.vertices.size());
    center.y /= static_cast<float>(mesh.vertices.size());
    for (auto& v : mesh.vertices) {
        v.x -= center.x;
        v.y -= center.y;
    }
    mesh.origin.x += center.x;
    mesh.origin.y += center.y;
    updateBounds();
}

void Drawable::copyFromDrawable(const Drawable& src) {
    mesh = src.mesh;
    deformationOffsets = src.deformationOffsets;
    vertexOffset = src.vertexOffset;
    uvOffset = src.uvOffset;
    deformOffset = src.deformOffset;
    tint = src.tint;
    screenTint = src.screenTint;
    emissionStrength = src.emissionStrength;
    textures = src.textures;
    bounds = src.bounds;
    weldedTargets = src.weldedTargets;
    weldedLinks = src.weldedLinks;
    welded = src.welded;
}

void Drawable::setupChildDrawable() {}
void Drawable::releaseChildDrawable() {}

void Drawable::buildDrawable(bool force) {
    if (force || !mesh.isReady()) {
        updateVertices();
        updateBounds();
    }
}

bool Drawable::mustPropagateDrawable() const { return true; }

void Drawable::fillDrawPacket(const Node& header, PartDrawPacket& packet, bool /*isMask*/) const {
    packet.modelMatrix = header.transform().toMat4();
    packet.renderable = mesh.isReady();
}

void Drawable::registerWeldFilter(const std::shared_ptr<Drawable>& target) {
    if (!target) return;
    FilterHook hook;
    hook.stage = 2;
    std::weak_ptr<Drawable> weakTarget = target;
    hook.func = [weakTarget, this](std::shared_ptr<Node> self, const std::vector<Vec2>& verts, const std::vector<Vec2>& deform, const Mat4* mat) {
        auto tgt = weakTarget.lock();
        if (!tgt) return std::make_tuple(std::vector<Vec2>{}, std::optional<Mat4>{}, false);
        Vec2Array v; v.resize(verts.size());
        for (std::size_t i = 0; i < verts.size(); ++i) { v.x[i] = verts[i].x; v.y[i] = verts[i].y; }
        Vec2Array d; d.resize(deform.size());
        for (std::size_t i = 0; i < deform.size(); ++i) { d.x[i] = deform[i].x; d.y[i] = deform[i].y; }
        auto res = weldingProcessor(tgt, v, d, mat);
        std::vector<Vec2> out;
        const auto& def = std::get<0>(res);
        out.reserve(def.size());
        for (std::size_t i = 0; i < def.size(); ++i) out.push_back(Vec2{def.x[i], def.y[i]});
        return std::make_tuple(out, std::optional<Mat4>{}, std::get<2>(res));
    };
    postProcessFilters.push_back(std::move(hook));
}

void Drawable::unregisterWeldFilter(const std::shared_ptr<Drawable>& target) {
    if (!target) return;
    postProcessFilters.erase(std::remove_if(postProcessFilters.begin(), postProcessFilters.end(), [&](const FilterHook& h) {
        // no reliable identification; drop all stage 2 weld hooks for this target
        return h.stage == 2;
    }), postProcessFilters.end());
}

void Drawable::serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const {
    Node::serializeSelfImpl(serializer, recursive, flags);
    if (has_flag(flags, SerializeNodeFlags::Geometry)) {
        serializer.putKey("mesh");
        serializer.putValue(std::string{"mesh_not_serialized"});
    }
    if (has_flag(flags, SerializeNodeFlags::Links) && !weldedLinks.empty()) {
        serializer.putKey("weldedLinks");
        auto arr = serializer.listBegin();
        for (const auto& link : weldedLinks) {
            serializer.elemBegin();
            serializer.putKey("targetUUID");
            serializer.putValue(link.target);
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
    if (auto wl = data.get_child_optional("weldedLinks")) {
        weldedLinks.clear();
        for (const auto& child : *wl) {
            WeldingLink link;
            link.target = child.second.get<uint32_t>("targetUUID", 0);
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
