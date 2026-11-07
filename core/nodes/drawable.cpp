// Independent nijilive.core.nodes.drawable source adaptation.
// Copyright (c) 2020, Inochi2D Project; (c) 2024, nijigenerate Project.
// BSD-2-Clause; full terms and disclaimer are retained in LICENSE.
#include "drawable.hpp"

#include "node.hpp"
#include "dynamic_composite.hpp"
#include "../puppet.hpp"

#include "../serde.hpp"
#include "../render/common.hpp"
#include "../render/shared_deform_buffer.hpp"
#include "../render/resource_lifetime_internal.hpp"
#include "../compat/scope_exit.hpp"
#include "../render/commands.hpp"
#include "../math/triangle.hpp"
#include "../math/mat3.hpp"
#include "../compat/phobos_string_hash.hpp"
#include "../compat/static_error.hpp"
#include "../compat/system_error.hpp"
#include "../compat/phobos_stdio.hpp"
#include "../runtime_state.hpp"
#include "../dbg.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <map>
#include <cmath>
#include <limits>
#include <optional>
#include <new>
#include <stdexcept>

namespace nicxlive::core::nodes {

namespace {
thread_local bool gDoGenerateBounds = false;
constexpr std::uintptr_t kNodeAttachFilterTag = 0x6e617474u; // 'natt'
constexpr std::uintptr_t kWeldFilterTagBase = 0x776c6400u;   // 'wld\0'
struct WeldingSizeHash {
    std::size_t operator()(std::size_t value) const noexcept { return value; }
};

using nicxlive::core::common::transformAdd;
using nicxlive::core::common::transformAssign;

// One actual compiled method context is shared by add, setup, removal and execution.
struct WeldCall {
    std::shared_ptr<Drawable> context;
    Node::DeformFilterResult operator()(std::shared_ptr<Node> node, const Vec2Array& vertices,
            Vec2Array& deformation, Mat4* matrix) const {
        using SourceMethod = Node::DeformFilterResult (Drawable::*)(
            std::shared_ptr<Node>, Vec2Array, Vec2Array, Mat4*);
        const auto method = static_cast<SourceMethod>(&Drawable::weldingProcessor);
        const auto inputVertices = vertices;
        const auto inputDeformation = deformation;
        return std::invoke(method, context.get(), node, inputVertices, inputDeformation, matrix);
    }
};

struct NodeAttachCall {
    std::shared_ptr<Drawable> context;
    std::tuple<Vec2Array, Mat4*, bool> operator()(std::shared_ptr<Node> node,
            const Vec2Array& vertices, Vec2Array deformation, const Mat4* matrix) const {
        const auto inputVertices = vertices;
        return context->nodeAttachProcessor(node, inputVertices, deformation, matrix);
    }
};

struct NodeWeldCall {
    std::shared_ptr<Drawable> context;
    std::tuple<Vec2Array, Mat4*, bool> operator()(std::shared_ptr<Node> node,
            const Vec2Array& vertices, Vec2Array deformation, const Mat4* matrix) const {
        // Node and Deformable are the two existing execution views of Source filters.
        auto mutableMatrix = const_cast<Mat4*>(matrix);
        const auto value = WeldCall{context}(node, vertices, deformation, mutableMatrix);
        if (value.transform) *d_runtime::ngRequireObject(mutableMatrix) = *value.transform;
        return {value.deformation, value.transform ? mutableMatrix : nullptr, value.changed};
    }
};

struct DeformAttachCall {
    std::shared_ptr<Drawable> context;
    Node::DeformFilterResult operator()(std::shared_ptr<Node> node, const Vec2Array& vertices,
            Vec2Array& deformation, Mat4* matrix) const {
        const auto inputVertices = vertices;
        const auto inputDeformation = deformation;
        const auto value = context->nodeAttachProcessor(node, inputVertices, inputDeformation, matrix);
        Node::DeformFilterResult result;
        result.deformation = std::get<0>(value);
        if (const auto transform = std::get<1>(value)) result.transform = *transform;
        result.changed = std::get<2>(value);
        return result;
    }
};

// Source stable-remove lowering follows Phobos std/range/primitives.d (BSL-1.0).
// Copyright Digital Mars 2000-2015. Distributed under the Boost Software License,
// Version 1.0. See https://www.boost.org/LICENSE_1_0.txt and the project's notices.
template<class Element>
common::SharedArray<Element> removeWeldSourceElement(common::SharedArray<Element> range,
        std::size_t position) {
#ifndef NDEBUG
    const auto failAssertion = [] {
        throw d_runtime::ngStaticError<d_runtime::AssertError>(
            d_runtime::RetainedString::fromStaticLiteral("range is smaller than amount of items to pop"),
            d_runtime::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2137);
    };
    if (position > range.size()) failAssertion();
#endif
    if (position > range.size()) throw d_runtime::ngStaticError<d_runtime::ArraySliceError>(
        position, range.size(), range.size(),
        d_runtime::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
    auto source = range.slice(position, range.size());
    auto destination = source;
#ifndef NDEBUG
    if (source.empty()) failAssertion();
#endif
    if (source.empty()) throw d_runtime::ngStaticError<d_runtime::ArraySliceError>(
        1, source.size(), source.size(),
        d_runtime::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
    source = source.slice(1, source.size());
    const auto result = range.slice(0, range.size() - 1);
    for (std::size_t index = 0; index < source.size(); ++index) destination[index] = source[index];
    return result;
}

bool sameWeldCall(const Node::DeformFilterHook& hook, const std::shared_ptr<Drawable>& owner) {
    const auto callable = hook.func.target<WeldCall>();
    return hook.stage == 2 && callable && callable->context.get() == owner.get();
}

void installWeldCall(Deformable::DeformFilterArray& storage, const std::shared_ptr<Drawable>& owner) {
    const auto actual = d_runtime::ngRequireObject(owner);
    auto filters = storage;
    for (const auto filter : filters) if (sameWeldCall(filter, actual)) return;
    Node::DeformFilterHook filter;
    filter.stage = 2;
    // Compatibility metadata only; actual Source equality uses the real method context.
    filter.tag = kWeldFilterTagBase | static_cast<std::uintptr_t>(actual->uuid);
    filter.func = WeldCall{actual};
    filters.push_back(filter);
    storage = filters;
}

void removeWeldCall(Deformable::DeformFilterArray& storage, const std::shared_ptr<Drawable>& owner) {
    const auto actual = d_runtime::ngRequireObject(owner);
    const auto filters = storage;
    for (std::size_t index = 0; index < filters.size(); ++index) {
        const auto filter = filters[index];
        if (!sameWeldCall(filter, actual)) continue;
        storage = removeWeldSourceElement(filters, index);
        break;
    }
}

void installWeldCall(Node::FilterArray& storage, const std::shared_ptr<Drawable>& owner) {
    const auto actual = d_runtime::ngRequireObject(owner);
    const auto filters = storage;
    for (const auto filter : filters) {
        const auto call = filter.func.target<NodeWeldCall>();
        if (filter.stage == 2 && call && call->context.get() == actual.get()) return;
    }
    Node::FilterHook filter;
    filter.stage = 2;
    filter.tag = kWeldFilterTagBase | static_cast<std::uintptr_t>(actual->uuid);
    filter.func = NodeWeldCall{actual};
    storage.push_back(filter);
}

void removeWeldCall(Node::FilterArray& storage, const std::shared_ptr<Drawable>& owner) {
    const auto actual = d_runtime::ngRequireObject(owner);
    const auto filters = storage;
    for (std::size_t index = 0; index < filters.size(); ++index) {
        const auto filter = filters[index];
        const auto call = filter.func.target<NodeWeldCall>();
        if (filter.stage != 2 || !call || call->context.get() != actual.get()) continue;
        storage = removeWeldSourceElement(filters, index);
        break;
    }
}

} // namespace

Drawable::Drawable() : Deformable() {
    bool constructed = false;
    d_runtime::ngRunWithScopeExit([&] {
        ::nicxlive::core::render::sharedDeformRegister(deformation, &deformOffset);
        ::nicxlive::core::render::sharedVertexRegister(mesh->vertices, &vertexOffset);
        ::nicxlive::core::render::sharedUvRegister(mesh->uvs, &uvOffset);
        const auto backend = core::currentRenderBackend();
        backend->createDrawableBuffers(ibo);
        drawableBackend = backend;
        constructed = true;
    }, [&] {
        if (constructed) return;
        // Failed native construction must not leave atlas references to dead fields.
        ::nicxlive::core::render::detail::enqueueDrawableRelease(drawableBackend, ibo);
        ::nicxlive::core::render::sharedDeformUnregister(deformation);
        ::nicxlive::core::render::sharedVertexUnregister(mesh->vertices);
        ::nicxlive::core::render::sharedUvUnregister(mesh->uvs);
    });
}

Drawable::Drawable(const std::shared_ptr<Node>& parent) : Deformable(parent) {
    bool constructed = false;
    d_runtime::ngRunWithScopeExit([&] {
        ::nicxlive::core::render::sharedDeformRegister(deformation, &deformOffset);
        ::nicxlive::core::render::sharedVertexRegister(mesh->vertices, &vertexOffset);
        ::nicxlive::core::render::sharedUvRegister(mesh->uvs, &uvOffset);
        const auto backend = core::currentRenderBackend();
        backend->createDrawableBuffers(ibo);
        drawableBackend = backend;
        constructed = true;
    }, [&] {
        if (constructed) return;
        // Failed native construction must not leave atlas references to dead fields.
        ::nicxlive::core::render::detail::enqueueDrawableRelease(drawableBackend, ibo);
        ::nicxlive::core::render::sharedDeformUnregister(deformation);
        ::nicxlive::core::render::sharedVertexUnregister(mesh->vertices);
        ::nicxlive::core::render::sharedUvUnregister(mesh->uvs);
    });
}

Drawable::Drawable(const MeshData& data, uint32_t uuidVal, const std::shared_ptr<Node>& parent)
    : Deformable(uuidVal, parent), mesh(std::make_shared<MeshData>(data)) {
    bool constructed = false;
    d_runtime::ngRunWithScopeExit([&] {
        ::nicxlive::core::render::sharedDeformRegister(deformation, &deformOffset);
        ::nicxlive::core::render::sharedVertexRegister(mesh->vertices, &vertexOffset);
        ::nicxlive::core::render::sharedUvRegister(mesh->uvs, &uvOffset);
        mesh->vertices = data.vertices.dup();
        const auto backend = core::currentRenderBackend();
        backend->createDrawableBuffers(ibo);
        drawableBackend = backend;
        updateIndices();
        updateVertices();
        constructed = true;
    }, [&] {
        if (constructed) return;
        // Failed native construction must not leave atlas references to dead fields.
        ::nicxlive::core::render::detail::enqueueDrawableRelease(drawableBackend, ibo);
        ::nicxlive::core::render::sharedDeformUnregister(deformation);
        ::nicxlive::core::render::sharedVertexUnregister(mesh->vertices);
        ::nicxlive::core::render::sharedUvUnregister(mesh->uvs);
    });
}

void Drawable::constructDrawableOwned(std::shared_ptr<Node> parent) {
    constructDeformableOwned(parent);
    ::nicxlive::core::render::sharedDeformRegister(deformation, &deformOffset);
    ::nicxlive::core::render::sharedVertexRegister(mesh->vertices, &vertexOffset);
    ::nicxlive::core::render::sharedUvRegister(mesh->uvs, &uvOffset);
    const auto backend = core::currentRenderBackend();
    backend->createDrawableBuffers(ibo);
    drawableBackend = backend;
}

void Drawable::constructDrawableDataOwned(MeshData data, std::uint32_t id, std::shared_ptr<Node> parent) {
    constructDeformableOwned(id, parent);
    *d_runtime::ngRequireObject(mesh) = data;
    ::nicxlive::core::render::sharedDeformRegister(deformation, &deformOffset);
    ::nicxlive::core::render::sharedVertexRegister(mesh->vertices, &vertexOffset);
    ::nicxlive::core::render::sharedUvRegister(mesh->uvs, &uvOffset);
    auto& destination = getVertices();
    destination = data.vertices.dup();
    const auto backend = core::currentRenderBackend();
    backend->createDrawableBuffers(ibo);
    drawableBackend = backend;
    updateIndices();
    updateVertices();
}

Drawable::~Drawable() {
    ::nicxlive::core::render::detail::enqueueDrawableRelease(drawableBackend, ibo);
    ::nicxlive::core::render::sharedDeformUnregister(deformation);
    ::nicxlive::core::render::sharedVertexUnregister(mesh->vertices);
    ::nicxlive::core::render::sharedUvUnregister(mesh->uvs);
}

namespace {
// TypeInfo_StaticArray.getHash -> getArrayHash -> hashOf(raw bytes),
// object.d1218/3877 and core.internal.hash193-215, BSL-1.0.
using MeshGridKey = std::array<std::int32_t, 2>;
struct MeshGridKeyPolicy {
    struct Hash {
        std::size_t operator()(const MeshGridKey& key) const noexcept {
            return d_runtime::ngPhobosStringHash(std::string_view(
                reinterpret_cast<const char*>(key.data()), sizeof(std::int32_t) * key.size()));
        }
    };
    struct Equal {
        bool operator()(const MeshGridKey& left, const MeshGridKey& right) const noexcept {
            return left == right;
        }
    };
};
using MeshGridMap = common::PolicyKeyMap<MeshGridKey, std::uint16_t, MeshGridKeyPolicy>;
template<class Array> decltype(auto) meshSourceAt(Array& values, std::size_t index, std::size_t line) {
    if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
        index, values.size(), d_runtime::RetainedString::fromStaticLiteral("nijilive/core/meshdata.d"), line);
    return values[index];
}
Vec2 meshSourcePoint(const Vec2Array& values, std::size_t index) {
#ifndef NDEBUG
    if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::AssertError>(
        "veca index out of range", "source/nijilive/math/veca.d", 139);
#endif
    if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
        index, values.size(), d_runtime::RetainedString::fromStaticLiteral("source/nijilive/math/veca.d"), 467);
    return {values.xAt(index), values.yAt(index)};
}
std::int32_t meshSourceNarrow(std::size_t value) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}
std::int32_t meshSourceIncrement(std::int32_t value) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value) + 1u);
}
std::int32_t meshSourceCoordinate(std::int32_t value, std::int32_t step, std::int32_t origin) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value) *
        static_cast<std::uint32_t>(step) - static_cast<std::uint32_t>(origin));
}
std::uint16_t meshSourceLookup(MeshGridMap& values, MeshGridKey key, std::size_t line) {
    const auto found = values.find(key);
    if (found == values.end()) throw d_runtime::ngStaticError<d_runtime::RangeError>(
        d_runtime::RetainedString::fromStaticLiteral("nijilive/core/meshdata.d"), line);
    return found->second;
}
}

void MeshData::add(Vec2 vertex, Vec2 uv) {
    vertices.push_back(vertex);
    uvs.push_back(uv);
}

void MeshData::clearConnections() {
    indices.resize(0);
}

void MeshData::connect(uint16_t first, uint16_t second) {
    indices.insertRange(indices.size(), {first, second});
}

int MeshData::find(Vec2 vert) const {
    const auto values = vertices;
    const auto length = values.size();
    for (std::size_t i = 0; i < length; ++i) {
        const auto value = meshSourcePoint(values, i);
        if (value.x == vert.x && value.y == vert.y) return meshSourceNarrow(i);
    }
    return -1;
}

bool MeshData::isReady() const {
    if (indices.empty() || indices.size() % 3 != 0) {
        return false;
    }
    std::size_t maxIdx = 0;
    const auto values = indices;
    for (auto v : values) {
        if (v > maxIdx) maxIdx = v;
    }
    return static_cast<size_t>(maxIdx) < vertices.size();
}

bool MeshData::canTriangulate() const {
    return !indices.empty() && indices.size() % 3 == 0;
}

void MeshData::fixWinding() {
    if (!isReady()) return;
    const auto count = indices.size() / 3;
    for (std::size_t j = 0; j < count; ++j) {
        const auto i = j * 3;
        Vec2 a = meshSourcePoint(vertices, meshSourceAt(indices, i, 105));
        Vec2 b = meshSourcePoint(vertices, meshSourceAt(indices, i + 1, 106));
        Vec2 c = meshSourcePoint(vertices, meshSourceAt(indices, i + 2, 107));
        float crossZ = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        bool cw = crossZ < 0;
        if (cw) {
            const auto temporary = meshSourceAt(indices, i + 1, 112);
            auto& destination = meshSourceAt(indices, i + 1, 113);
            const auto value = meshSourceAt(indices, i + 2, 113);
            destination = value;
            meshSourceAt(indices, i + 2, 114) = temporary;
        }
    }
}

int MeshData::connectionsAtPoint(Vec2 pt) const {
    int p = find(pt);
    if (p == -1) return 0;
    return connectionsAtPoint(static_cast<uint16_t>(p));
}

int MeshData::connectionsAtPoint(uint16_t idx) const {
    std::uint32_t found = 0;
    const auto values = indices;
    for (auto v : values) {
        if (v == idx) found++;
    }
    return std::bit_cast<std::int32_t>(found);
}

MeshData MeshData::copy() const {
    MeshData out;
    out.vertices = vertices.duplicate();
    out.uvs = uvs.duplicate();
    out.indices = indices.duplicate();
    out.gridAxes = gridAxes;
    out.origin = origin;
    return out;
}

// Current independent nijilive/core/meshdata.d grid operations, BSD-2-Clause.
MeshData MeshData::createQuadMesh(math::Vec2i size, Vec4 uvBounds, math::Vec2i cuts, math::Vec2i origin) {
    if (cuts.x < 2) cuts.x = 2;
    if (cuts.y < 2) cuts.y = 2;
    MeshData data;
    MeshGridMap mapping;
    const auto sw = size.x / cuts.x;
    const auto sh = size.y / cuts.y;
    const float uvx = uvBounds.w / static_cast<float>(cuts.x);
    const float uvy = uvBounds.z / static_cast<float>(cuts.y);
    const auto yEnd = meshSourceIncrement(cuts.y);
    for (std::int32_t y = 0; y < yEnd; ++y) {
        // The original does not pre-size gridAxes. Its bounds failure is real.
        meshSourceAt(data.gridAxes, 0, 270).push_back(
            static_cast<float>(meshSourceCoordinate(y, sh, origin.y)));
        const auto xEnd = meshSourceIncrement(cuts.x);
        for (std::int32_t x = 0; x < xEnd; ++x) {
            meshSourceAt(data.gridAxes, 1, 272).push_back(
                static_cast<float>(meshSourceCoordinate(x, sw, origin.x)));
            data.vertices.push_back(Vec2{
                static_cast<float>(meshSourceCoordinate(x, sw, origin.x)),
                static_cast<float>(meshSourceCoordinate(y, sh, origin.y))});
            data.uvs.push_back(Vec2{uvBounds.x + static_cast<float>(x) * uvx,
                uvBounds.y + static_cast<float>(y) * uvy});
            mapping[MeshGridKey{x, y}] = static_cast<std::uint16_t>(data.vertices.size() - 1);
        }
    }
    const math::Vec2i center{cuts.x / 2, cuts.y / 2};
    for (std::int32_t y = 0; y < cuts.y; ++y) {
        for (std::int32_t x = 0; x < cuts.x; ++x) {
            const MeshGridKey p0{x, y}, p1{x, meshSourceIncrement(y)},
                p2{meshSourceIncrement(x), y}, p3{meshSourceIncrement(x), meshSourceIncrement(y)};
            if ((x < center.x && y < center.y) || (x >= center.x && y >= center.y)) {
                common::SharedArray<std::uint16_t> values{
                    meshSourceLookup(mapping, p0, 299), meshSourceLookup(mapping, p2, 300),
                    meshSourceLookup(mapping, p3, 301), meshSourceLookup(mapping, p0, 302),
                    meshSourceLookup(mapping, p3, 303), meshSourceLookup(mapping, p1, 304)};
                data.indices.insertRange(data.indices.size(), values);
            } else {
                common::SharedArray<std::uint16_t> values{
                    meshSourceLookup(mapping, p0, 308), meshSourceLookup(mapping, p1, 309),
                    meshSourceLookup(mapping, p2, 310), meshSourceLookup(mapping, p1, 311),
                    meshSourceLookup(mapping, p2, 312), meshSourceLookup(mapping, p3, 313)};
                data.indices.insertRange(data.indices.size(), values);
            }
        }
    }
    return data;
}

bool MeshData::isGrid() const {
    return gridAxes.size() == 2 && gridAxes[0].size() > 2 && gridAxes[1].size() > 2;
}

bool MeshData::clearGridIfDirty() {
    if (gridAxes.size() < 2 || gridAxes[0].empty() || gridAxes[1].empty()) return false;
    auto clearGrid = [&]() {
        // Mutate the two retained descriptors, preserving the outer array and its aliases.
        gridAxes[0].resize(0);
        gridAxes[1].resize(0);
        return true;
    };
    if (vertices.size() != gridAxes[0].size() * gridAxes[1].size()) return clearGrid();
    std::uint32_t indexBits = 0;
    const auto yValues = meshSourceAt(gridAxes, 0, 341);
    for (float y : yValues) {
        const auto xValues = meshSourceAt(gridAxes, 1, 342);
        for (float x : xValues) {
            const auto index = static_cast<std::size_t>(std::bit_cast<std::int32_t>(indexBits));
            const auto vertex = meshSourcePoint(vertices, index);
            if (x != vertex.x || y != vertex.y) return clearGrid();
            ++indexBits;
        }
    }
    return false;
}

bool MeshData::regenerateGrid() {
    // SharedArray indexing preserves the source bounds failure when an axis is absent.
    if (meshSourceAt(gridAxes, 0, 354).size() < 2 || meshSourceAt(gridAxes, 1, 354).size() < 2) return false;
    vertices.resize(0);
    uvs.resize(0);
    indices.resize(0);
    MeshGridMap mapping;
    const float minY = meshSourceAt(meshSourceAt(gridAxes, 0, 363), 0, 363);
    const float maxY = meshSourceAt(meshSourceAt(gridAxes, 0, 363),
        meshSourceAt(gridAxes, 0, 363).size() - 1, 363);
    const float minX = meshSourceAt(meshSourceAt(gridAxes, 1, 364), 0, 364);
    const float maxX = meshSourceAt(meshSourceAt(gridAxes, 1, 364),
        meshSourceAt(gridAxes, 1, 364).size() - 1, 364);
    // Preserve the source's axis naming and denominators; do not normalize or swap them.
    const float width = maxY - minY;
    const float height = maxX - minX;
    const auto yValues = meshSourceAt(gridAxes, 0, 367);
    for (std::size_t i = 0; i < yValues.size(); ++i) {
        const float y = yValues[i];
        const auto xValues = meshSourceAt(gridAxes, 1, 368);
        for (std::size_t j = 0; j < xValues.size(); ++j) {
            const float x = xValues[j];
            vertices.push_back(Vec2{x, y});
            uvs.push_back(Vec2{(x - minX) / width, (y - minY) / height});
            mapping[MeshGridKey{meshSourceNarrow(j), meshSourceNarrow(i)}] =
                static_cast<std::uint16_t>(vertices.size() - 1);
        }
    }
    const Vec2 center{minX + width / 2, minY + height / 2};
    const auto yEnd = meshSourceAt(gridAxes, 0, 376).size() - 1;
    for (std::size_t i = 0; i < yEnd; ++i) {
        const float yValue = meshSourceAt(meshSourceAt(gridAxes, 0, 377), i, 377);
        const auto xEnd = meshSourceAt(gridAxes, 1, 378).size() - 1;
        for (std::size_t j = 0; j < xEnd; ++j) {
            const float xValue = meshSourceAt(meshSourceAt(gridAxes, 1, 380), j, 380);
            const auto x = meshSourceNarrow(j), y = meshSourceNarrow(i);
            const MeshGridKey p0{x, y}, p1{x, meshSourceIncrement(y)},
                p2{meshSourceIncrement(x), y}, p3{meshSourceIncrement(x), meshSourceIncrement(y)};
            if ((xValue < center.x && yValue < center.y) || (xValue >= center.x && yValue >= center.y)) {
                common::SharedArray<std::uint16_t> values{
                    meshSourceLookup(mapping, p0, 392), meshSourceLookup(mapping, p2, 393),
                    meshSourceLookup(mapping, p3, 394), meshSourceLookup(mapping, p0, 395),
                    meshSourceLookup(mapping, p3, 396), meshSourceLookup(mapping, p1, 397)};
                indices.insertRange(indices.size(), values);
            } else {
                common::SharedArray<std::uint16_t> values{
                    meshSourceLookup(mapping, p0, 401), meshSourceLookup(mapping, p1, 402),
                    meshSourceLookup(mapping, p2, 403), meshSourceLookup(mapping, p1, 404),
                    meshSourceLookup(mapping, p2, 405), meshSourceLookup(mapping, p3, 406)};
                indices.insertRange(indices.size(), values);
            }
        }
    }
    return true;
}

void MeshData::dbg() {
#ifndef NDEBUG
    // All original writefln arguments are evaluated before stdout is acquired.
    const auto vertexCount = vertices.size();
    const auto uvCount = uvs.size();
    const auto indexCount = indices.size();
    namespace stdio = ::nicxlive::core::d_runtime::phobos::stdio;
    stdio::standardOutput().withTextWriter([=](stdio::FileTextWriter& writer) {
        const auto number = [&](std::size_t value) {
            // Source's fixed %s unsigned-size format has no width or precision.
            char bytes[std::numeric_limits<std::size_t>::digits10 + 1];
            const auto result = std::to_chars(bytes, bytes + sizeof(bytes), value);
            writer.put(std::string_view(bytes, static_cast<std::size_t>(result.ptr - bytes)));
        };
        number(vertexCount); writer.put(std::string_view(" "));
        number(uvCount); writer.put(std::string_view(" "));
        number(indexCount); writer.put(static_cast<unsigned char>('\n'));
    });
#endif
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
            gridAxes.push_back(common::SharedArray<float>(std::move(vals)));
        }
    }

    return std::nullopt;
}

void MeshData::serialize(::nicxlive::core::serde::TypedJsonWriter& serializer) const {
    const auto object = serializer.structBegin();
    auto pairs = [&](const char* key, const auto& values) {
        serializer.putKey(key);
        const auto array = serializer.listBegin();
        for (const auto& value : values) {
            serializer.elemBegin(); serializer.putValue(value.x);
            serializer.elemBegin(); serializer.putValue(value.y);
        }
        serializer.listEnd(array);
    };
    pairs("verts", vertices);
    if (!uvs.empty()) pairs("uvs", uvs);
    serializer.putField("indices", indices);
    serializer.putField("origin", std::array<float, 2>{origin.x, origin.y});
    if (gridAxes.size() == 2 && gridAxes[0].size() > 2 && gridAxes[1].size() > 2)
        serializer.putField("grid_axes", gridAxes);
    serializer.structEnd(object);
}

::nicxlive::core::serde::ReadError MeshData::deserializeFromFghj(::nicxlive::core::serde::TypedJsonView data) {
    namespace serde = ::nicxlive::core::serde;
    if (data.isMissing()) return {};
    auto pairs = [&](const char* key, auto& values) {
        auto entries = data.findFirst(key).byElement();
        while (!entries.empty()) {
            float x = std::numeric_limits<float>::quiet_NaN();
            float y = std::numeric_limits<float>::quiet_NaN();
            (void)serde::deserializeValue(entries.front(), x);
            entries.popFront();
            // D evaluates front of the exhausted range for an odd-length list.
            (void)serde::deserializeValue(entries.front(), y);
            entries.popFront();
            values.push_back(Vec2{x, y});
        }
    };
    pairs("verts", vertices);
    const auto uv = data.findFirst("uvs");
    if (!uv.isMissing()) pairs("uvs", uvs);
    const auto org = data.findFirst("origin");
    if (!org.isMissing()) {
        if (org.kind() != serde::JsonKind::Null) {
            if (org.kind() != serde::JsonKind::Array) throw serde::JsonConversionError(
                d_runtime::RetainedString(std::string_view("Expected a numeric vector array")),
                d_runtime::RetainedString::fromStaticLiteral("nijilive/math/serialization.d"), 36);
            auto entries = org.byElement();
            std::size_t component = 0;
            while (!entries.empty() && component < 2) {
                auto& destination = component ? origin.y : origin.x;
                if (serde::deserializeNumber(entries.front(), destination)) break;
                ++component;
                entries.popFront();
            }
        }
    }
    // D length=0 preserves an existing non-null slice descriptor.
    gridAxes.resize(0);
    const auto axes = data.findFirst("grid_axes");
    if (!axes.isMissing()) (void)serde::deserializeValue(axes, gridAxes);
    for (const auto entry : data.findFirst("indices").byElement()) {
        uint16_t index = 0;
        (void)serde::deserializeValue(entry, index);
        indices.push_back(index);
    }
    return {};
}

void WeldingLink::serialize(::nicxlive::core::serde::TypedJsonWriter& serializer) const {
    const auto object = serializer.structBegin();
    serializer.putField("targetUUID", targetUUID);
    serializer.putField("indices", indices);
    serializer.putField("weight", weight);
    serializer.structEnd(object);
}

::nicxlive::core::serde::ReadError WeldingLink::deserializeFromFghj(::nicxlive::core::serde::TypedJsonView data) {
    namespace serde = ::nicxlive::core::serde;
    if (data.kind() != serde::JsonKind::Object) {
        static constexpr const char* names[] = {"null_", "false_", "number", "string", "array", "object"};
        const auto kind = data.kind() == serde::JsonKind::Boolean && data.boolean() ? "true_" :
            names[static_cast<unsigned>(data.kind())];
        return "Cann't deserialize WeldingLink. Unexpected data: " + std::string(kind);
    }
    unsigned seen = 0;
    for (const auto& [key, field] : data.members()) {
        const unsigned flag = key == "targetUUID" ? 1 : key == "indices" ? 2 : key == "weight" ? 4 : 0;
        if (seen & flag)
            return "ASDF deserialisation: multiple keys for member '" + key + "' in WeldingLink are not allowed.";
        seen |= flag;
        serde::ReadError error;
        if (key == "targetUUID") error = serde::deserializeValue(field, targetUUID);
        else if (key == "indices") error = serde::deserializeValue(field, indices);
        else if (key == "weight") error = serde::deserializeValue(field, weight);
        if (error) return error;
    }
    for (const auto& [key, flag] : {std::pair{"targetUUID", 1u}, std::pair{"indices", 2u}, std::pair{"weight", 4u}})
        if (!(seen & flag))
            return "FGHJ deserialisation: non-optional member '" + std::string(key) + "' in WeldingLink is missing.";
    return {};
}

void Drawable::updateBounds() {
    if (!gDoGenerateBounds) return;
    const auto matrix = getDynamicMatrix();
    if (getVertices().empty()) {
        const auto worldTransform = transform();
        bounds = std::array<float, 4>{worldTransform.translation.x, worldTransform.translation.y,
            worldTransform.translation.x, worldTransform.translation.y};
        return;
    }
    const auto point = [&](std::size_t index) {
        const Vec2 vertex = getVertices().at(index);
        const Vec2 offset = deformation.at(index);
        return matrix.transformPoint({vertex.x + offset.x, vertex.y + offset.y, 0});
    };
    const auto first = point(0);
    bounds = std::array<float, 4>{first.x, first.y, first.x, first.y};
    auto& currentBounds = *bounds;
    for (std::size_t index = 1; index < getVertices().size(); ++index) {
        const auto oriented = point(index);
        if (oriented.x < (*bounds)[0]) (*bounds)[0] = oriented.x;
        if (oriented.y < (*bounds)[1]) (*bounds)[1] = oriented.y;
        if (oriented.x > (*bounds)[2]) (*bounds)[2] = oriented.x;
        if (oriented.y > (*bounds)[3]) (*bounds)[3] = oriented.y;
    }
}

void Drawable::drawMeshLines(math::Vec4 color) const {
    if (getVertices().empty() || mesh->indices.empty()) return;
    Mat4 trans = getDynamicMatrix();
    if (auto ot = getOneTimeTransform()) trans = Mat4::multiply(*ot, trans);
    const auto indices = mesh->indices;
    // D Vec3Array length initializes the trailing lanes to zero too.
    std::vector<Vec3> points(indices.size() * 2, Vec3{0, 0, 0});
    const auto point = [this](std::size_t index) {
        const Vec2 vertex = getVertices().at(index);
        const Vec2 offset = deformation.at(index);
        return Vec3{vertex.x - mesh->origin.x + offset.x,
            vertex.y - mesh->origin.y + offset.y, 0};
    };
    for (std::size_t i = 0; i < indices.size() / 3; ++i) {
        const auto ix = i * 3;
        const auto iy = ix * 2;
        const auto index = indices[ix];
        points[iy] = point(index);
        points[iy + 1] = point(indices[ix + 1]);
        points[iy + 2] = point(indices[ix + 1]);
        points[iy + 3] = point(indices[ix + 2]);
        points[iy + 4] = point(indices[ix + 2]);
        points[iy + 5] = point(index);
    }
    // Use the Source debug path: it creates sequential edge indices and
    // applies the active camera before drawing the local mesh coordinates.
    core::dbg::inDbgSetBuffer(std::span<const Vec3>(points));
    core::dbg::inDbgDrawLines(color, trans);
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
Mat4 Drawable::meshOverlayMatrix() {
    auto trans = getDynamicMatrix();
    if (auto once = getOneTimeTransform()) trans = Mat4::multiply(*once, trans);
    return trans;
}
Vec2Array Drawable::meshOverlayPoints() {
    const auto self = std::static_pointer_cast<Drawable>(shared_from_this());
    Vec2Array points;
    points.setLength(getVertices().size());
    getVertices().forEachValue([&](std::size_t index, Vec2 point) {
        const auto origin = d_runtime::ngRequireObject(mesh)->origin;
        const Vec2 relative{point.x - origin.x, point.y - origin.y};
        const auto offset = deformation[index].toVector();
        const Vec2 result{relative.x + offset.x, relative.y + offset.y};
        points[index] = result;
        return 0;
    });
    return points;
}
MeshData& Drawable::getMesh() { return *mesh; }

void Drawable::updateIndices() {
    const auto backend = core::currentRenderBackend();
    const auto handle = ibo;
    const auto indices = d_runtime::ngRequireObject(mesh)->indices;
    backend->uploadDrawableIndices(handle, indices);
}

std::tuple<Vec2Array, Mat4*, bool> Drawable::nodeAttachProcessor(const std::shared_ptr<Node>& node,
                                                                               const Vec2Array& origVertices,
                                                                               Vec2Array origDeformation,
                                                                               const Mat4* origTransform) {
    (void)origVertices;
    (void)origDeformation;
    (void)origTransform;
    bool changed = updateAttachedNodeTransform(node);
    return {Vec2Array{}, nullptr, changed};
}

bool Drawable::updateAttachedNodeTransform(const std::shared_ptr<Node>& node) {
    const auto lifetime = shared_from_this();
    const auto target = node;
    const Mat4 inverseMatrix = transform().matrix().inverse();
    const Vec3 translation = d_runtime::ngRequireObject(target)->transform().translation;
    const float input[]{translation.x, translation.y, translation.z, 1.0f};
    Vec2 nodeOrigin{0, 0};
    float* output[]{&nodeOrigin.x, &nodeOrigin.y};
    for (std::size_t column = 0; column < 4; ++column)
        for (std::size_t row = 0; row < 2; ++row)
            *output[row] = math::detail::inmathAccumulate(*output[row],
                math::detail::inmathProduct(inverseMatrix.a.a[row][column], input[column]));
    if (!attachedIndex.contains(target)) {
        const auto triangle = math::findSurroundingTriangle(nodeOrigin, mesh);
        attachedIndex[target] = triangle;
    }
    const auto triangle = attachedIndex.at(target);
    if (triangle.empty()) return false;
    Vec2 transformedOrigin;
    float rotateVertical, rotateHorizontal;
    const auto sourceVertices = d_runtime::ngRequireObject(mesh)->vertices;
    const auto sourceDeformation = deformation;
    math::nlCalculateTransformInTriangle(sourceVertices, triangle, sourceDeformation,
        nodeOrigin, transformedOrigin, rotateVertical, rotateHorizontal);
    transformedOrigin.x -= nodeOrigin.x;
    transformedOrigin.y -= nodeOrigin.y;
    target->setValueSource(d_runtime::RetainedString::fromStaticLiteral("transform.t.x"), transformedOrigin.x);
    target->setValueSource(d_runtime::RetainedString::fromStaticLiteral("transform.t.y"), transformedOrigin.y);
    target->setValueSource(d_runtime::RetainedString::fromStaticLiteral("transform.r.z"),
        (rotateHorizontal + rotateVertical) / 2.0f);
    transformChanged();
    return true;
}

void Drawable::updateVertices() {
    sharedVertexResize(mesh->vertices, mesh->vertices.size());
    sharedVertexMarkDirty();
    sharedDeformResize(deformation, getVertices().size());
    deformation.fill(Vec2{0.0f, 0.0f});
    updateDeform();
}

void Drawable::rebufferMesh(const MeshData& data) {
    sharedVertexResize(mesh->vertices, data.vertices.size());
    sharedUvResize(mesh->uvs, data.uvs.size());
    *mesh = data;
    updateIndices();
    updateVertices();
}

void Drawable::rebuffer(const MeshData& data) {
    Drawable::rebufferMesh(data);
}

void Drawable::updateDeform() {
    Deformable::updateDeform();
    sharedDeformMarkDirty();
    updateBounds();
}

void Drawable::reset() {
    getVertices().sliceAssign(0, mesh->vertices);
}

bool Drawable::isWeldedBy(const std::shared_ptr<Drawable>& target) {
    const auto lifetime = weak_from_this().lock();
    const auto targetOwner = target;
    const auto links = weldedLinks;
    for (const auto link : links) if (ngResourceObjectEquals(link.target.lock(), targetOwner)) return true;
    return false;
}

bool Drawable::isWeldedBy(NodeId target) const {
    return std::any_of(weldedLinks.begin(), weldedLinks.end(), [&](const WeldingLink& l) { return l.targetUUID == target; });
}

Node::DeformFilterResult Drawable::weldingProcessor(const std::shared_ptr<Node>& target,
                                                    const Vec2Array& origVertices,
                                                    Vec2Array& origDeformation,
                                                    const Mat4* origTransform) {
    // Preserve the legacy signature while executing the single actual Source body.
    const auto owner = target;
    const auto vertices = origVertices;
    const auto values = origDeformation;
    using SourceMethod = Node::DeformFilterResult (Drawable::*)(
        std::shared_ptr<Node>, Vec2Array, Vec2Array, Mat4*);
    const auto method = static_cast<SourceMethod>(&Drawable::weldingProcessor);
    auto result = std::invoke(method, this, owner, vertices, values, const_cast<Mat4*>(origTransform));
    if (!result.deformation.empty()) origDeformation = result.deformation;
    return result;
}

// Complete independent Source welding processor; the legacy native overload stays available.
Node::DeformFilterResult Drawable::weldingProcessor(std::shared_ptr<Node> target,
        Vec2Array origVertices, Vec2Array origDeformation, Mat4* origTransform) {
    const auto self = std::static_pointer_cast<Drawable>(shared_from_this());
    const auto finish = [](Vec2Array values, bool changed) {
        Node::DeformFilterResult result;
        result.changed = changed;
        result.deformation = values;
        return result;
    };
    const auto checked = [](auto& values, std::size_t index, std::size_t line) -> decltype(auto) {
        if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
            index, values.size(),
            d_runtime::RetainedString::fromStaticLiteral("source/nijilive/core/nodes/drawable.d"), line);
        return values[index];
    };
    const auto find = [](const common::SharedArray<WeldingLink> links,
                         const std::shared_ptr<Resource>& other) {
        for (std::size_t index = 0; index < links.size(); ++index) {
            const auto candidate = links[index];
            if (ngResourceObjectEquals(candidate.target.lock(), other))
                return static_cast<std::ptrdiff_t>(index);
        }
        return std::ptrdiff_t{-1};
    };
    const auto linkIndex = find(weldedLinks, target);
    bool changed = false;
    if (linkIndex < 0) return finish(origDeformation, false);
    const WeldingLink link = checked(weldedLinks, static_cast<std::size_t>(linkIndex), 116);
    const auto linked = d_runtime::ngRequireObject(link.target.lock());
    const auto reciprocal = find(linked->weldedLinks, self);
    if (reciprocal < 0) return finish(origDeformation, false);
    const auto counterIndices = checked(linked->weldedLinks,
        static_cast<std::size_t>(reciprocal), 119).indices;
    if (postProcessed < 2) return finish({}, changed);
    if ((weldingApplied.contains(linked) && weldingApplied.at(linked)) ||
        (linked->weldingApplied.contains(self) && linked->weldingApplied.at(self))) return finish({}, changed);
    weldingApplied[linked] = true;
    linked->weldingApplied[self] = true;
    const float nonnegative = 0.0f < link.weight ? link.weight : 0.0f;
    const float weldingWeight = nonnegative < 1.0f ? nonnegative : 1.0f;
    const auto firstVertexCount = getVertices().size();
    const auto pairCount = link.indices.size() < firstVertexCount
        ? link.indices.size() : getVertices().size();
    if (!pairCount) return finish(origDeformation, changed);

    std::vector<std::size_t> selfIndices, targetIndices;
    selfIndices.reserve(pairCount);
    targetIndices.reserve(pairCount);
    for (std::size_t index = 0; index < pairCount; ++index) {
        const auto mapped = checked(link.indices, index, 140);
        if (mapped == NOINDEX || mapped < 0) continue;
        const auto targetIndex = static_cast<std::size_t>(mapped);
        if (targetIndex >= origVertices.size()) continue;
        if (targetIndex >= counterIndices.size() || counterIndices[targetIndex] < 0) continue;
        selfIndices.push_back(index);
        targetIndices.push_back(targetIndex);
    }

    const auto counterIndex = find(linked->weldedLinks, self);
    if (counterIndex >= 0) {
        std::vector<std::size_t> counterSelf, counterTarget;
        const auto indices = checked(linked->weldedLinks,
            static_cast<std::size_t>(counterIndex), 157).indices;
        for (std::size_t index = 0; index < indices.size(); ++index) {
            const auto mapped = indices[index];
            if (index < origVertices.size() && mapped >= 0 &&
                static_cast<std::size_t>(mapped) < getVertices().size() &&
                static_cast<std::size_t>(mapped) < link.indices.size() &&
                link.indices[static_cast<std::size_t>(mapped)] >= 0) {
                counterSelf.push_back(static_cast<std::size_t>(mapped));
                counterTarget.push_back(index);
            }
        }
        bool useCounter = counterSelf.size() > selfIndices.size();
        if (!useCounter && counterSelf.size() == selfIndices.size()) {
            const auto ownUuid = getUUID();
            const auto targetUuid = linked->getUUID();
            useCounter = ownUuid > targetUuid;
        }
        if (useCounter) {
            selfIndices = std::move(counterSelf);
            targetIndices = std::move(counterTarget);
        }
    }
    const auto validCount = selfIndices.size();
    if (!validCount) return finish(origDeformation, false);

    Vec2Array selfLocal = gatherVec2(getVertices(), selfIndices);
    Vec2Array selfDelta = gatherVec2(deformation, selfIndices);
    selfLocal += selfDelta;
    Vec2Array targetLocal = gatherVec2(origVertices, targetIndices);
    Vec2Array targetDelta = gatherVec2(origDeformation, targetIndices);
    targetLocal += targetDelta;
    const Mat4 selfMatrix = overrideTransformMatrix ? *overrideTransformMatrix : transform().matrix();
    const Mat4 targetMatrix = *d_runtime::ngRequireObject(origTransform);
    Vec2Array selfWorld, targetWorld;
    transformAssign(selfWorld, selfLocal, selfMatrix);
    transformAssign(targetWorld, targetLocal, targetMatrix);

    std::vector<std::size_t> groups(validCount);
    for (std::size_t index = 0; index < validCount; ++index) groups[index] = index;
    const auto root = [&](std::size_t index) {
        while (groups[index] != index) index = groups[index];
        return index;
    };
    using FirstMap = common::ResourceKeyMap<std::size_t, std::size_t,
        WeldingSizeHash, std::equal_to<std::size_t>>;
    FirstMap firstSelf, firstTarget;
    for (std::size_t index = 0; index < validCount; ++index) {
        if (const auto previous = firstSelf.find(selfIndices[index]); previous != firstSelf.end())
            groups[root(index)] = root(previous->second);
        else firstSelf[selfIndices[index]] = index;
        if (const auto previous = firstTarget.find(targetIndices[index]); previous != firstTarget.end())
            groups[root(index)] = root(previous->second);
        else firstTarget[targetIndices[index]] = index;
    }
    std::vector<Vec2> selfSum(validCount, Vec2{0, 0}), targetSum(validCount, Vec2{0, 0});
    std::vector<std::size_t> selfCount(validCount), targetCount(validCount);
    for (std::size_t index = 0; index < validCount; ++index) {
        const auto group = root(index);
        if (firstSelf.at(selfIndices[index]) == index) {
            const Vec2 point = selfWorld[index];
            selfSum[group].x += point.x;
            selfSum[group].y += point.y;
            ++selfCount[group];
        }
        if (firstTarget.at(targetIndices[index]) == index) {
            const Vec2 point = targetWorld[index];
            targetSum[group].x += point.x;
            targetSum[group].y += point.y;
            ++targetCount[group];
        }
    }
    Vec2Array blended(validCount);
    for (std::size_t index = 0; index < validCount; ++index) {
        const auto group = root(index);
        const float selfSize = static_cast<float>(selfCount[group]);
        const float targetSize = static_cast<float>(targetCount[group]);
        const Vec2 value{
            selfSum[group].x / selfSize * weldingWeight +
                targetSum[group].x / targetSize * (1.0f - weldingWeight),
            selfSum[group].y / selfSize * weldingWeight +
                targetSum[group].y / targetSize * (1.0f - weldingWeight)};
        blended.set(index, value);
    }
    Vec2Array deltaSelf = blended.dup();
    deltaSelf -= selfWorld;
    Vec2Array deltaTarget = blended.dup();
    deltaTarget -= targetWorld;
    Mat4 selfInverse = selfMatrix.inverse();
    selfInverse[0][3] = selfInverse[1][3] = selfInverse[2][3] = 0;
    Mat4 targetInverse = targetMatrix.inverse();
    targetInverse[0][3] = targetInverse[1][3] = targetInverse[2][3] = 0;
    Vec2Array localSelf = makeZeroVecArray(validCount);
    transformAdd(localSelf, deltaSelf, selfInverse);
    Vec2Array localTarget = makeZeroVecArray(validCount);
    transformAdd(localTarget, deltaTarget, targetInverse);
    scatterAddVec2(localSelf, selfIndices, deformation, changed);
    scatterAddVec2(localTarget, targetIndices, origDeformation, changed);
    sharedDeformMarkDirty();
    return finish(origDeformation, changed);
}

Vec2Array Drawable::gatherVec2(Vec2Array source, std::span<const std::size_t> indices) {
    Vec2Array result(indices.size());
    for (std::size_t index = 0; index < indices.size(); ++index) {
        const auto sourceIndex = indices[index];
        if (sourceIndex >= source.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
            sourceIndex, source.size(),
            d_runtime::RetainedString::fromStaticLiteral("source/nijilive/core/nodes/drawable.d"), 431);
        result.xAt(index) = source.xAt(sourceIndex);
        result.yAt(index) = source.yAt(sourceIndex);
    }
    return result;
}

Vec2Array Drawable::makeZeroVecArray(std::size_t count) {
    Vec2Array result(count);
    for (std::size_t index = 0; index < count; ++index) result.xAt(index) = 0;
    for (std::size_t index = 0; index < count; ++index) result.yAt(index) = 0;
    return result;
}

void Drawable::scatterAddVec2(Vec2Array delta, std::span<const std::size_t> indices,
        Vec2Array& target, bool& changed) {
    std::vector<std::size_t> counts(target.size());
    std::vector<Vec2> sums(target.size(), Vec2{0, 0});
    for (std::size_t index = 0; index < indices.size(); ++index) {
        const auto destination = indices[index];
        if (destination >= counts.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
            destination, counts.size(),
            d_runtime::RetainedString::fromStaticLiteral("source/nijilive/core/nodes/drawable.d"), 457);
        ++counts[destination];
        if (index >= delta.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
            index, delta.size(),
            d_runtime::RetainedString::fromStaticLiteral("source/nijilive/core/nodes/drawable.d"), 458);
        sums[destination].x += delta.xAt(index);
        sums[destination].y += delta.yAt(index);
    }
    for (std::size_t index = 0; index < target.size(); ++index) {
        if (!counts[index]) continue;
        const float count = static_cast<float>(counts[index]);
        const float x = sums[index].x / count, y = sums[index].y / count;
        if (x != 0 || y != 0) changed = true;
        target.xAt(index) += x;
        target.yAt(index) += y;
    }
}

common::SharedArray<std::ptrdiff_t> Drawable::counterWeldingIndices(
    const std::shared_ptr<Drawable>& target, std::span<const std::ptrdiff_t> indices) {
    if (!target || target.get() == this) throw std::runtime_error("Welding requires a distinct target");
    if (indices.size() != getVertices().size()) throw std::runtime_error("Welding vertex count does not match mesh");
    for (auto index : indices) {
        if (index == NOINDEX) continue;
        if (index < 0 || static_cast<std::size_t>(index) >= target->getVertices().size())
            throw std::runtime_error("Welding target vertex index is out of range");
    }
    common::SharedArray<std::ptrdiff_t> result(target->getVertices().size(), NOINDEX);
    std::vector<Vec2> anchors(getVertices().size());
    for (std::size_t i = 0; i < getVertices().size(); ++i) {
        const auto vertex = getVertices()[i].toVector();
        const auto matrix = transform().matrix();
        float x = 0, y = 0;
        const float components[4] = {vertex.x, vertex.y, 0, 1};
        for (std::size_t k = 0; k < 4; ++k) {
            x += matrix.a.a[0][k] * components[k]; y += matrix.a.a[1][k] * components[k];
        }
        anchors[i] = {x, y};
    }
    for (std::size_t j = 0; j < target->getVertices().size(); ++j) {
        const auto vertex = target->getVertices()[j].toVector();
        const auto matrix = target->transform().matrix();
        float x = 0, y = 0;
        const float components[4] = {vertex.x, vertex.y, 0, 1};
        for (std::size_t k = 0; k < 4; ++k) {
            x += matrix.a.a[0][k] * components[k]; y += matrix.a.a[1][k] * components[k];
        }
        const Vec2 point{x, y};
        float nearest = 16;
        for (std::size_t i = 0; i < anchors.size(); ++i) {
            if (indices[i] == NOINDEX) continue;
            const Vec2 delta{point.x - anchors[i].x, point.y - anchors[i].y};
            const float distance = delta.x * delta.x + delta.y * delta.y;
            if (distance < nearest) { nearest = distance; result[j] = static_cast<std::ptrdiff_t>(i); }
        }
    }
    return result;
}

common::SharedArray<std::ptrdiff_t> Drawable::updatedCounterWeldingIndices(
    const std::shared_ptr<Drawable>& target, std::span<const std::ptrdiff_t> indices) {
    if (indices.size() != getVertices().size()) throw std::runtime_error("Welding vertex count does not match mesh");
    auto own = std::find_if(weldedLinks.begin(), weldedLinks.end(),
        [&](const WeldingLink& link) { return link.target.lock() == target; });
    auto counter = std::find_if(target->weldedLinks.begin(), target->weldedLinks.end(),
        [&](const WeldingLink& link) { return link.target.lock().get() == this; });
    if (own == weldedLinks.end() || counter == target->weldedLinks.end())
        throw std::runtime_error("Welding requires both endpoint links");
    auto result = counter->indices.duplicate();
    for (auto& index : result) {
        if (index < 0) continue;
        const auto i = static_cast<std::size_t>(index);
        if (i >= indices.size() || i >= own->indices.size() || indices[i] < 0 || indices[i] != own->indices[i])
            index = NOINDEX;
    }
    for (std::size_t i = 0; i < indices.size(); ++i) {
        const auto j = indices[i];
        if (j == NOINDEX) continue;
        if (j < 0 || static_cast<std::size_t>(j) >= result.size())
            throw std::runtime_error("Welding target vertex index is out of range");
        if (result[static_cast<std::size_t>(j)] < 0) result[static_cast<std::size_t>(j)] = static_cast<std::ptrdiff_t>(i);
    }
    return result;
}

common::SharedArray<std::ptrdiff_t> Drawable::updatedCounterWeldingIndices(
    std::shared_ptr<Drawable> target, const common::SharedArray<std::ptrdiff_t> indices) {
    const auto self = std::static_pointer_cast<Drawable>(shared_from_this());
    static constexpr char sourceFile[] = "source/nijilive/core/nodes/drawable.d";
    const auto enforce = [](bool condition, const char* message, std::size_t line) {
        if (!condition) throw d_runtime::ThrowableException(
            d_runtime::RetainedString(std::string(message)),
            d_runtime::RetainedString::fromStaticLiteral(sourceFile), line);
    };
    const auto checkedLink = [](const common::SharedArray<WeldingLink>& links,
                               std::size_t index, std::size_t line) -> const WeldingLink& {
        if (index >= links.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
            index, links.size(), d_runtime::RetainedString::fromStaticLiteral(sourceFile), line);
        return links[index];
    };
    enforce(indices.size() == getVertices().size(),
        "Welding vertex count does not match mesh", 737);

    std::ptrdiff_t own = -1;
    const auto ownRange = weldedLinks;
    for (std::size_t index = 0; index < ownRange.size(); ++index) {
        const auto candidate = ownRange[index];
        if (ngResourceObjectEquals(candidate.target.lock(), target)) {
            own = static_cast<std::ptrdiff_t>(index);
            break;
        }
    }
    std::ptrdiff_t counter = -1;
    const auto counterRange = d_runtime::ngRequireObject(target)->weldedLinks;
    for (std::size_t index = 0; index < counterRange.size(); ++index) {
        const auto candidate = counterRange[index];
        if (ngResourceObjectEquals(candidate.target.lock(), self)) {
            counter = static_cast<std::ptrdiff_t>(index);
            break;
        }
    }
    enforce(own >= 0 && counter >= 0, "Welding requires both endpoint links", 740);
    auto result = checkedLink(d_runtime::ngRequireObject(target)->weldedLinks,
        static_cast<std::size_t>(counter), 741).indices.duplicate();
    for (auto& value : result) {
        if (value < 0) continue;
        const auto index = static_cast<std::size_t>(value);
        if (index >= indices.size() ||
            index >= checkedLink(weldedLinks, static_cast<std::size_t>(own), 744).indices.size() ||
            indices[index] < 0 ||
            indices[index] != checkedLink(weldedLinks,
                static_cast<std::size_t>(own), 745).indices[index]) value = NOINDEX;
    }
    for (std::size_t index = 0; index < indices.size(); ++index) {
        const auto targetIndex = indices[index];
        if (targetIndex == NOINDEX) continue;
        enforce(targetIndex >= 0 && static_cast<std::size_t>(targetIndex) < result.size(),
            "Welding target vertex index is out of range", 750);
        const auto slot = static_cast<std::size_t>(targetIndex);
        if (result[slot] < 0) result[slot] = static_cast<std::ptrdiff_t>(index);
    }
    return result;
}

void Drawable::addWeldedTarget(const std::shared_ptr<Drawable>& target,
    const std::vector<std::ptrdiff_t>& indices, float weight) {
    auto counterIndices = counterWeldingIndices(target, indices);
    auto own = std::find_if(weldedLinks.begin(), weldedLinks.end(),
        [&](const WeldingLink& link) { return link.target.lock() == target; });
    if (own != weldedLinks.end()) own->indices = common::SharedArray<std::ptrdiff_t>(indices);
    else weldedLinks.push_back(WeldingLink{target->uuid, target, common::SharedArray<std::ptrdiff_t>(indices), weight});
    auto counter = std::find_if(target->weldedLinks.begin(), target->weldedLinks.end(),
        [&](const WeldingLink& link) { return link.target.lock().get() == this; });
    if (counter != target->weldedLinks.end()) counter->indices = counterIndices;
    else target->weldedLinks.push_back(WeldingLink{uuid,
        std::dynamic_pointer_cast<Drawable>(shared_from_this()), counterIndices, 1.0f - weight});
    // Compatibility caches mirror the real links; they do not select link identity.
    if (std::find(weldedTargets.begin(), weldedTargets.end(), target->uuid) == weldedTargets.end())
        weldedTargets.push_back(target->uuid);
    if (std::find(target->weldedTargets.begin(), target->weldedTargets.end(), uuid) == target->weldedTargets.end())
        target->weldedTargets.push_back(uuid);
    welded = target->welded = true;
    target->registerWeldFilter(std::dynamic_pointer_cast<Drawable>(shared_from_this()));
    registerWeldFilter(target);
}

common::SharedArray<std::ptrdiff_t> Drawable::counterWeldingIndices(
    std::shared_ptr<Drawable> target, const common::SharedArray<std::ptrdiff_t> indices) {
    const auto self = std::static_pointer_cast<Drawable>(shared_from_this());
    static constexpr char sourceFile[] = "source/nijilive/core/nodes/drawable.d";
    const auto enforce = [](bool condition, const char* message, std::size_t line) {
        if (!condition) throw d_runtime::ThrowableException(
            d_runtime::RetainedString(std::string(message)),
            d_runtime::RetainedString::fromStaticLiteral(sourceFile), line);
    };
    const auto checked = [](auto& values, std::size_t index, std::size_t line) -> decltype(auto) {
        if (index >= values.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
            index, values.size(), d_runtime::RetainedString::fromStaticLiteral(sourceFile), line);
        return values[index];
    };
    enforce(target && target.get() != this, "Welding requires a distinct target", 684);
    enforce(indices.size() == getVertices().size(), "Welding vertex count does not match mesh", 685);
    for (const auto index : indices) {
        if (index == NOINDEX) continue;
        enforce(index >= 0 && static_cast<std::size_t>(index) < target->getVertices().size(),
            "Welding target vertex index is out of range", 688);
    }
    common::SharedArray<std::ptrdiff_t> result(target->getVertices().size(), 0);
    for (auto& value : result) value = NOINDEX;
    common::SharedArray<Vec2> anchors(getVertices().size(), Vec2{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()});
    const auto project = [](const Mat4& matrix, const Vec2 vertex) {
        Vec2 point{0, 0};
        const float components[4]{vertex.x, vertex.y, 0, 1};
        for (std::size_t column = 0; column < 4; ++column) {
            point.x = math::detail::inmathAccumulate(point.x,
                math::detail::inmathProduct(matrix.a.a[0][column], components[column]));
            point.y = math::detail::inmathAccumulate(point.y,
                math::detail::inmathProduct(matrix.a.a[1][column], components[column]));
        }
        return point;
    };
    getVertices().forEachValue([&](std::size_t index, Vec2 vertex) {
        const auto matrix = transform().matrix();
        checked(anchors, index, 694) = project(matrix, vertex);
        return 0;
    });
    target->getVertices().forEachValue([&](std::size_t targetIndex, Vec2 vertex) {
        const auto matrix = target->transform().matrix();
        const auto point = project(matrix, vertex);
        float nearest = 16;
        for (std::size_t index = 0; index < anchors.size(); ++index) {
            const auto anchor = anchors[index];
            if (checked(indices, index, 699) == NOINDEX) continue;
            const Vec2 delta{point.x - anchor.x, point.y - anchor.y};
            const float distance = delta.x * delta.x + delta.y * delta.y;
            if (distance < nearest) {
                nearest = distance;
                checked(result, targetIndex, 704) = static_cast<std::ptrdiff_t>(index);
            }
        }
        return 0;
    });
    return result;
}

void Drawable::addWeldedTarget(std::shared_ptr<Drawable> target,
    common::SharedArray<std::ptrdiff_t> indices, float weight) {
    const auto self = std::static_pointer_cast<Drawable>(shared_from_this());
    auto counterIndices = counterWeldingIndices(target, indices);
    const auto find = [](const common::SharedArray<WeldingLink> links,
                         const std::shared_ptr<Drawable>& other) {
        for (std::size_t index = 0; index < links.size(); ++index) {
            const auto candidate = links[index];
            if (ngResourceObjectEquals(candidate.target.lock(), other))
                return static_cast<std::ptrdiff_t>(index);
        }
        return std::ptrdiff_t{-1};
    };
    const auto checked = [](auto& links, std::size_t index, std::size_t line) -> WeldingLink& {
        if (index >= links.size()) throw d_runtime::ngStaticError<d_runtime::ArrayIndexError>(
            index, links.size(),
            d_runtime::RetainedString::fromStaticLiteral("source/nijilive/core/nodes/drawable.d"), line);
        return links[index];
    };
    const auto ownIndex = find(weldedLinks, target);
    if (ownIndex >= 0) {
        auto links = weldedLinks;
        auto& destination = checked(links, static_cast<std::size_t>(ownIndex), 716).indices;
        auto duplicate = indices.duplicate();
        destination = duplicate;
    } else {
        const auto targetUuid = d_runtime::ngRequireObject(target)->getUUID();
        auto duplicate = indices.duplicate();
        const WeldingLink link{targetUuid, target, duplicate, weight};
        weldedLinks.push_back(link);
    }
    const auto counterIndex = find(d_runtime::ngRequireObject(target)->weldedLinks, self);
    if (counterIndex >= 0) {
        checked(d_runtime::ngRequireObject(target)->weldedLinks,
            static_cast<std::size_t>(counterIndex), 725).indices = counterIndices;
    } else {
        const auto ownUuid = getUUID();
        const WeldingLink link{ownUuid, self, counterIndices, 1.0f - weight};
        d_runtime::ngRequireObject(target)->weldedLinks.push_back(link);
    }
    installWeldCall(d_runtime::ngRequireObject(target)->deformPostProcessFilters, self);
    installWeldCall(target->postProcessFilters, self);
    installWeldCall(deformPostProcessFilters, target);
    installWeldCall(postProcessFilters, target);
}


void Drawable::removeWeldedTarget(const std::shared_ptr<Drawable>& target) {
    const auto targetOwner = target;
    const auto self = std::static_pointer_cast<Drawable>(shared_from_this());
    const auto find = [](const common::SharedArray<WeldingLink> links,
                         const std::shared_ptr<Drawable>& other) {
        for (std::size_t index = 0; index < links.size(); ++index) {
            const auto link = links[index];
            if (ngResourceObjectEquals(link.target.lock(), other)) return static_cast<std::ptrdiff_t>(index);
        }
        return std::ptrdiff_t{-1};
    };
    auto index = find(weldedLinks, targetOwner);
    if (index >= 0) {
        weldedLinks = removeWeldSourceElement(weldedLinks, static_cast<std::size_t>(index));
        removeWeldCall(deformPostProcessFilters, targetOwner);
        removeWeldCall(postProcessFilters, targetOwner);
    }
    index = find(d_runtime::ngRequireObject(targetOwner)->weldedLinks, self);
    if (index >= 0) {
        d_runtime::ngRequireObject(targetOwner)->weldedLinks = removeWeldSourceElement(
            d_runtime::ngRequireObject(targetOwner)->weldedLinks, static_cast<std::size_t>(index));
        removeWeldCall(d_runtime::ngRequireObject(targetOwner)->deformPostProcessFilters, self);
        removeWeldCall(targetOwner->postProcessFilters, self);
    }
    // Existing native UUID caches are not the Source link/delegate identity.
    weldedTargets.erase(std::remove(weldedTargets.begin(), weldedTargets.end(),
        d_runtime::ngRequireObject(targetOwner)->uuid), weldedTargets.end());
    auto& targetCache = d_runtime::ngRequireObject(targetOwner)->weldedTargets;
    targetCache.erase(std::remove(targetCache.begin(), targetCache.end(), uuid), targetCache.end());
}

void Drawable::setupSelf() {
    const auto lifetime = weak_from_this().lock();
    const auto links = weldedLinks;
    for (const auto link : links) {
        const auto target = link.target.lock();
        installWeldCall(deformPostProcessFilters, target);
        installWeldCall(postProcessFilters, target);
    }
}

void Drawable::finalize() {
    Deformable::finalize();
    finalizeDrawable();
    setupSelf();
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
    weldedLinks = valid.empty() ? common::SharedArray<WeldingLink>{} : common::SharedArray<WeldingLink>(std::move(valid));
    buildDrawable(true);
}

void Drawable::clearCache() {
    attachedIndex.clear();
}

void Drawable::runBeginTask(core::RenderContext& ctx) {
    weldingApplied.clear();
    const auto links = weldedLinks;
    for (const auto link : links) weldingApplied[link.target.lock()] = false;
    Deformable::runBeginTask(ctx);
}

void Drawable::runPostTaskImpl(std::size_t priority, core::RenderContext& ctx) {
    Deformable::runPostTaskImpl(priority, ctx);
}

void Drawable::normalizeUV(void* dataPointer) {
    auto& data = *static_cast<MeshData*>(dataPointer);
    if (data.uvs.empty()) return;
    float minX = data.uvs[0].x, maxX = minX;
    float minY = data.uvs[0].y, maxY = minY;
    for (std::size_t i = 1; i < data.uvs.size(); ++i) {
        const auto uv = data.uvs[i];
        minX = std::min(minX, uv.x.reference()); maxX = std::max(maxX, uv.x.reference());
        minY = std::min(minY, uv.y.reference()); maxY = std::max(maxY, uv.y.reference());
    }
    const float width = maxX - minX, height = maxY - minY;
    if (width == 0 || height == 0) {
        for (std::size_t i=0; i<data.uvs.size(); ++i) data.uvs.set(i, Vec2{0.5f, 0.5f});
        return;
    }
    const float centerX = (minX + maxX) / 2.0f / width;
    const float centerY = (minY + maxY) / 2.0f / height;
    for (std::size_t i=0; i<data.uvs.size(); ++i) {
        auto uv=data.uvs[i];
        uv.x /= width; uv.y /= height;
        uv.x += 0.5f - centerX; uv.y += 0.5f - centerY;
    }
}

void Drawable::normalizeUv() {
    if (mesh->uvs.empty()) return;
    float minX = mesh->uvs[0].x;
    float maxX = minX;
    float minY = mesh->uvs[0].y;
    float maxY = minY;
    for (std::size_t i = 1; i < mesh->uvs.size(); ++i) {
        const auto& uv = mesh->uvs[i];
        minX = std::min(minX, uv.x.reference());
        maxX = std::max(maxX, uv.x.reference());
        minY = std::min(minY, uv.y.reference());
        maxY = std::max(maxY, uv.y.reference());
    }
    float width = maxX - minX;
    float height = maxY - minY;
    float centerX = width != 0.0f ? (minX + maxX) / (2.0f * width) : 0.0f;
    float centerY = height != 0.0f ? (minY + maxY) / (2.0f * height) : 0.0f;
    for (std::size_t i=0; i<mesh->uvs.size(); ++i) {
        auto uv=mesh->uvs[i];
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
    MeshData copied;
    copied.vertices = src.mesh->vertices.dup();
    copied.uvs = src.mesh->uvs.empty() ? src.mesh->vertices.dup() : src.mesh->uvs.dup();
    copied.indices = src.mesh->indices;
    copied.origin = src.mesh->origin;
    copied.gridAxes = src.mesh->gridAxes;
    rebufferMesh(copied);
    deformation = src.deformation;
    tint = src.tint;
    screenTint = src.screenTint;
    emissionStrength = src.emissionStrength;
    textures = src.textures;
    bounds = src.bounds;
    weldedTargets = src.weldedTargets;
    weldedLinks = src.weldedLinks;
    weldingApplied = src.weldingApplied;
}

void Drawable::copyFrom(const Node& src, bool clone, bool deepCopy) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    [[maybe_unused]] const auto sourceLifetime = src.weak_from_this().lock();
    bool autoResized = false;
    auto* composite = dynamic_cast<DynamicComposite*>(&const_cast<Node&>(src));
    if (composite) {
        autoResized = composite->autoResizedMesh;
        composite->autoResizedMesh = false;
    }
    Deformable::copyFrom(src, clone, deepCopy);
    if (const auto* drawable = dynamic_cast<const Drawable*>(&src)) {
        MeshData copied;
        copied.vertices = d_runtime::ngRequireObject(drawable->mesh)->vertices.dup();
        copied.uvs = d_runtime::ngRequireObject(drawable->mesh)->uvs.dup();
        if (copied.uvs.size() != copied.vertices.size()) copied.uvs = copied.vertices.dup();
        copied.indices = d_runtime::ngRequireObject(drawable->mesh)->indices.duplicate();
        const auto axes = d_runtime::ngRequireObject(drawable->mesh)->gridAxes;
        for (const auto axis : axes) copied.gridAxes.push_back(axis.duplicate());
        deformation = drawable->deformation.dup();
        rebuffer(copied);
    }
    if (composite) composite->autoResizedMesh = autoResized;
}

bool Drawable::setupChild(const std::shared_ptr<Node>& child) {
    const auto owner = std::static_pointer_cast<Drawable>(shared_from_this());
    const auto target = child;
    Node::setupChild(target);
    if (d_runtime::ngRequireObject(target)->pinToMesh) {
        if (const auto deformable = std::dynamic_pointer_cast<Deformable>(target)) {
            const auto filters = deformable->deformPreProcessFilters;
            bool exists = false;
            for (const auto filter : filters) {
                const auto call = filter.func.target<DeformAttachCall>();
                if (filter.stage == 0 && call && call->context.get() == owner.get()) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                Node::DeformFilterHook filter;
                filter.stage = 0;
                filter.tag = kNodeAttachFilterTag;
                filter.func = DeformAttachCall{owner};
                deformable->deformPreProcessFilters.push_back(filter);
            }
        }
        // Projectable selects Node's view when autoResizedMesh changes at runtime.
        const auto filters = target->preProcessFilters;
        bool exists = false;
        for (const auto filter : filters) {
            const auto call = filter.func.target<NodeAttachCall>();
            if (filter.stage == 0 && call && call->context.get() == owner.get()) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            Node::FilterHook filter;
            filter.stage = 0;
            filter.tag = kNodeAttachFilterTag;
            filter.func = NodeAttachCall{owner};
            target->preProcessFilters.push_back(filter);
        }
    }
    return true;
}

void Drawable::setupChildDrawable() {
    for (auto& child : children) Drawable::setupChild(child);
}

bool Drawable::releaseChild(const std::shared_ptr<Node>& child) {
    const auto owner = std::static_pointer_cast<Drawable>(shared_from_this());
    const auto target = d_runtime::ngRequireObject(child);
    if (const auto deformable = std::dynamic_pointer_cast<Deformable>(target)) {
        const auto filters = deformable->deformPreProcessFilters;
        for (std::size_t index = 0; index < filters.size(); ++index) {
            const auto filter = filters[index];
            const auto call = filter.func.target<DeformAttachCall>();
            if (filter.stage != 0 || !call || call->context.get() != owner.get()) continue;
            deformable->deformPreProcessFilters = removeWeldSourceElement(filters, index);
            break;
        }
    }
    {
        const auto filters = target->preProcessFilters;
        for (std::size_t index = 0; index < filters.size(); ++index) {
            const auto filter = filters[index];
            const auto call = filter.func.target<NodeAttachCall>();
            if (filter.stage != 0 || !call || call->context.get() != owner.get()) continue;
            target->preProcessFilters = removeWeldSourceElement(filters, index);
            break;
        }
    }
    Node::releaseChild(target);
    return true;
}

void Drawable::releaseChildDrawable() {
    for (auto& child : children) Drawable::releaseChild(child);
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

bool Drawable::mustPropagate() const { return mustPropagateDrawable(); }

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
    packet.indexBuffer = ibo;
    for (std::size_t i = 0; i < textures.size() && i < 3; ++i) {
        if (textures[i]) {
            uint32_t texId = textures[i]->getRuntimeUUID();
            if (texId == 0) texId = textures[i]->backendId();
            packet.textureUUIDs[i] = texId;
            packet.textureBackendIds[i] = textures[i]->backendId();
        } else {
            packet.textureUUIDs[i] = 0;
            packet.textureBackendIds[i] = 0;
        }
    }
    packet.node.reset();
}

void Drawable::registerWeldFilter(const std::shared_ptr<Drawable>& target) {
    if (!target) return; // Preserve the legacy private adapter's borrowed/null domain.
    const auto owner = target;
    installWeldCall(deformPostProcessFilters, owner);
    installWeldCall(postProcessFilters, owner);
}

void Drawable::unregisterWeldFilter(const std::shared_ptr<Drawable>& target) {
    if (!target) return;
    const auto owner = target;
    removeWeldCall(deformPostProcessFilters, owner);
    removeWeldCall(postProcessFilters, owner);
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
        getVertices().resize(mesh->vertices.size());
        for (std::size_t i = 0; i < mesh->vertices.size(); ++i) {
            getVertices().set(i, mesh->vertices[i]);
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

void Drawable::serializeSelfImpl(::nicxlive::core::serde::TypedJsonWriter& serializer,
    bool recursive, SerializeNodeFlags flags) const {
    Node::serializeSelfImpl(serializer, recursive, flags);
    if (has_flag(flags, SerializeNodeFlags::Geometry)) {
        serializer.putKey("mesh");
        if (!mesh) throw std::logic_error("Missing Drawable mesh storage");
        mesh->serialize(serializer);
    }
    if (has_flag(flags, SerializeNodeFlags::Links) && !weldedLinks.empty()) {
        // Validate every link before emitting the weldedLinks key or array.
        const auto failWelding = [](auto formatMessage, std::size_t line) {
            d_runtime::RetainedString message;
            try {
                message = d_runtime::RetainedString(formatMessage());
            } catch (const std::bad_alloc&) {
                throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(
                    d_runtime::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
            } catch (const std::length_error&) {
                throw d_runtime::ngStaticError<d_runtime::OutOfMemoryError>(
                    d_runtime::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
            }
            throw d_runtime::ThrowableException(std::move(message),
                d_runtime::RetainedString::fromStaticLiteral("source/nijilive/core/nodes/drawable.d"), line);
        };
        for (const auto& link : weldedLinks) {
            const auto target = link.target.lock();
            if (!target || link.indices.size() != getVertices().size()) {
                const auto sourceName = getName();
                failWelding([&] {
                    std::string message = "Cannot save Welding of '";
                    message.append(sourceName.view());
                    message += "': missing target or incorrect vertex count";
                    return message;
                }, 321);
            }
            const auto reciprocal = std::find_if(target->weldedLinks.begin(), target->weldedLinks.end(),
                [&](const WeldingLink& candidate) { return candidate.target.lock().get() == this; });
            if (reciprocal == target->weldedLinks.end()) {
                const auto sourceName = getName();
                failWelding([&] {
                    std::string message = "Cannot save Welding of '";
                    message.append(sourceName.view());
                    message += "': missing reciprocal link";
                    return message;
                }, 324);
            }
            for (std::size_t i = 0; i < link.indices.size(); ++i) {
                const auto index = link.indices[i];
                if (index == NOINDEX) continue;
                if (index < 0 || static_cast<std::size_t>(index) >= target->getVertices().size()) {
                    const auto sourceName = getName();
                    const auto targetName = target->getName();
                    failWelding([&] {
                        std::string message = "Cannot save Welding '";
                        message.append(sourceName.view());
                        message += "' -> '";
                        message.append(targetName.view());
                        message += "', vertex ";
                        message += std::to_string(i);
                        message += ": inconsistent mapping ";
                        message += std::to_string(index);
                        return message;
                    }, 328);
                }
            }
        }
        serializer.putKey("weldedLinks");
        const auto array = serializer.listBegin();
        for (const auto& link : weldedLinks) {
            serializer.elemBegin();
            link.serialize(serializer);
        }
        serializer.listEnd(array);
    }
}

::nicxlive::core::serde::ReadError Drawable::deserializeFromFghj(::nicxlive::core::serde::TypedJsonView data) {
    namespace serde = ::nicxlive::core::serde;
    (void)Node::deserializeFromFghj(data);
    if (!mesh) throw std::logic_error("Missing Drawable mesh storage");
    if (auto error = mesh->deserializeFromFghj(data.findFirst("mesh"))) return error;
    // The actual mesh vertex cell is registered with the atlas. Allocate that real backing
    // before copying the values of D's data.vertices.dup.
    sharedVertexResize(mesh->vertices, mesh->vertices.size());
    for (std::size_t i = 0; i < mesh->vertices.size(); ++i) getVertices().set(i, mesh->vertices[i]);
    const auto links = data.findFirst("weldedLinks");
    if (!links.isMissing()) (void)serde::deserializeValue(links, weldedLinks);
    updateIndices();
    updateVertices();
    return {};
}

} // namespace nicxlive::core::nodes
