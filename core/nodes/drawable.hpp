#pragma once
#include "../common/class_reference.hpp"
#include "../common/resource_key_map.hpp"
#include "../common/object_key_policy.hpp"

#include "deformable.hpp"
#include "filter.hpp"
#include "../texture.hpp"
#include "../compat/native_access_fault.hpp"
#include "../common/utils.hpp"
#include "../common/shared_array.hpp"
#include "../math/triangle.hpp"
#include "../math/mat3.hpp"
#include "../serde.hpp"
#include "../render/shared_deform_buffer.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <unordered_set>
#include <unordered_map>
#include <optional>
#include <array>
#include <memory>
#include <span>

namespace nicxlive::core::nodes {

// Shared buffer helpers (CPU-side emulation)
Vec2Array& sharedVertexBufferData();
Vec2Array& sharedUvBufferData();
Vec2Array& sharedDeformBufferData();
void sharedVertexResize(Vec2Array& target, std::size_t newLength);
void sharedUvResize(Vec2Array& target, std::size_t newLength);
void sharedUvResize(std::vector<Vec2>& target, std::size_t newLength);
void sharedDeformResize(Vec2Array& target, std::size_t newLength);
void sharedVertexMarkDirty();
void sharedUvMarkDirty();
void sharedDeformMarkDirty();
void inSetUpdateBounds(bool state);
bool inGetUpdateBounds();

struct MeshData {
    Vec2Array vertices{};
    Vec2Array uvs{};
    common::SharedArray<uint16_t> indices{};
    Vec2 origin{0.0f, 0.0f};
    common::SharedArray<common::SharedArray<float>> gridAxes{};

    void add(Vec2 vertex, Vec2 uv);
    void clearConnections();
    void connect(uint16_t first, uint16_t second);
    int find(Vec2 vert) const;
    bool isReady() const;
    bool canTriangulate() const;
    void fixWinding();
    int connectionsAtPoint(Vec2 pt) const;
    int connectionsAtPoint(uint16_t idx) const;
    MeshData copy() const;
    bool isGrid() const;
    bool clearGridIfDirty();
    bool regenerateGrid();
    void dbg();
    static MeshData createQuadMesh(math::Vec2i size, Vec4 uvBounds,
        math::Vec2i cuts = {6, 6}, math::Vec2i origin = {0, 0});

    template<class S> requires requires(S& serializer, d_runtime::RetainedString key) {
        serializer.putKey(key);
    }
    void serialize(S& serializer) {
        auto state = serializer.structBegin();
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("verts"));
        auto array = serializer.listBegin();
        {
            const auto values = vertices;
            const auto length = values.size();
            for (std::size_t i = 0; i < length; ++i) {
                Vec2 value = values[i];
                serializer.elemBegin(); serializer.serializeValue(value.x);
                serializer.elemBegin(); serializer.serializeValue(value.y);
            }
        }
        serializer.listEnd(array);
        if (!uvs.empty()) {
            serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("uvs"));
            array = serializer.listBegin();
            const auto values = uvs;
            const auto length = values.size();
            for (std::size_t i = 0; i < length; ++i) {
                Vec2 value = values[i];
                serializer.elemBegin(); serializer.serializeValue(value.x);
                serializer.elemBegin(); serializer.serializeValue(value.y);
            }
            serializer.listEnd(array);
        }
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("indices"));
        serializer.serializeValue(indices);
        serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("origin"));
        {
            Vec2 value = origin;
            auto vectorState = serializer.listBegin();
            serializer.elemBegin(); serializer.serializeValue(value.x);
            serializer.elemBegin(); serializer.serializeValue(value.y);
            serializer.listEnd(vectorState);
        }
        if (isGrid()) {
            serializer.putKey(d_runtime::RetainedString::fromStaticLiteral("grid_axes"));
            serializer.serializeValue(gridAxes);
        }
        serializer.structEnd(state);
    }

    void serialize(::nicxlive::core::serde::TypedJsonWriter& serializer) const;
    ::nicxlive::core::serde::ReadError deserializeFromFghj(::nicxlive::core::serde::TypedJsonView data);
    void serialize(::nicxlive::core::serde::InochiSerializer& serializer) const;
    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data);
};

static constexpr std::ptrdiff_t NOINDEX = static_cast<std::ptrdiff_t>(-1);

struct WeldingLink {
    NodeId targetUUID{};
    common::ClassReference<Drawable> target{};
    common::SharedArray<std::ptrdiff_t> indices{};
    float weight{std::numeric_limits<float>::quiet_NaN()};

    void serialize(::nicxlive::core::serde::TypedJsonWriter& serializer) const;
    ::nicxlive::core::serde::ReadError deserializeFromFghj(::nicxlive::core::serde::TypedJsonView data);
};

class Drawable : public Deformable {
public:
    virtual void renderMask(bool dodge = false) = 0;

    common::NullableString typeId() const override {
        return common::NullableString::fromStaticLiteral("Drawable");
    }
    std::shared_ptr<MeshData> mesh{std::make_shared<MeshData>()};
    Vec2Array sharedUvs{};
    std::vector<Vec2> deformationOffsets{};
    std::size_t vertexOffset{0};
    std::size_t uvOffset{0};
    std::size_t deformOffset{0};
    std::size_t ibo{0};
    std::weak_ptr<::nicxlive::core::RenderBackend> drawableBackend;
    Vec3 tint{};
    Vec3 screenTint{};
    float emissionStrength{0.0f};
    std::array<std::shared_ptr<::nicxlive::core::Texture>, 3> textures{};
    std::optional<std::array<float, 4>> bounds{std::array<float, 4>{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()}};

    std::vector<NodeId> weldedTargets{};
    bool welded{false};
    common::SharedArray<WeldingLink> weldedLinks{};
    using WeldingAppliedMap = common::PolicyKeyMap<std::shared_ptr<Drawable>, bool,
        common::ObjectClassKeyPolicy<std::shared_ptr<Drawable>>>;
    WeldingAppliedMap weldingApplied{};
    using AttachedIndexMap = common::PolicyKeyMap<std::shared_ptr<Node>, common::SharedArray<int>,
        common::ObjectClassKeyPolicy<std::shared_ptr<Node>>>;
    AttachedIndexMap attachedIndex{};

    Drawable();
    explicit Drawable(const std::shared_ptr<Node>& parent);
    Drawable(const MeshData& data, uint32_t uuidVal = 0, const std::shared_ptr<Node>& parent = nullptr);
    ~Drawable() override;

    virtual void updateIndices();

    virtual void updateVertices() override;
    virtual void updateDeform() override;

    virtual void updateBounds() override;

    virtual void drawMeshLines(math::Vec4 color = {0.5f, 0.5f, 0.5f, 1.0f}) const;
    virtual void drawMeshPoints() const;
    virtual Mat4 meshOverlayMatrix();
    virtual Vec2Array meshOverlayPoints();
    using Deformable::getVertices;
    Vec2Array& getVertices() override { return d_runtime::ngRequireObject(mesh)->vertices; }
    virtual MeshData& getMesh();

    virtual std::tuple<Vec2Array, Mat4*, bool> nodeAttachProcessor(const std::shared_ptr<Node>& node,
                                                                    const Vec2Array& origVertices,
                                                                    Vec2Array origDeformation,
                                                                    const Mat4* origTransform);

    virtual Node::DeformFilterResult weldingProcessor(const std::shared_ptr<Node>& target,
                                                      const Vec2Array& origVertices,
                                                      Vec2Array& origDeformation,
                                                      const Mat4* origTransform);

    virtual void rebufferMesh(const MeshData& data);

    virtual void reset();

    common::SharedArray<std::ptrdiff_t> counterWeldingIndices(
        const std::shared_ptr<Drawable>& target, std::span<const std::ptrdiff_t> indices);
    common::SharedArray<std::ptrdiff_t> updatedCounterWeldingIndices(
        const std::shared_ptr<Drawable>& target, std::span<const std::ptrdiff_t> indices);
    virtual void addWeldedTarget(const std::shared_ptr<Drawable>& target,
                                 const std::vector<std::ptrdiff_t>& indices,
                                 float weight);

    virtual void removeWeldedTarget(const std::shared_ptr<Drawable>& target);

    virtual bool isWeldedBy(NodeId target) const;
    virtual bool isWeldedBy(const std::shared_ptr<Drawable>& target);

    virtual void setupSelf() override;

    void finalize() override;
    virtual void finalizeDrawable();

    virtual void normalizeUv();
    void normalizeUV(void* data) override;

    virtual void clearCache() override;

    virtual void centralizeDrawable();

    virtual void copyFromDrawable(const Drawable& src);

    bool setupChild(const std::shared_ptr<Node>& child) override;
    bool releaseChild(const std::shared_ptr<Node>& child) override;

    virtual void setupChildDrawable();
    virtual void releaseChildDrawable();

    virtual void build(bool force) override;
    virtual void buildDrawable(bool force);

    bool mustPropagate() const override;
    virtual bool mustPropagateDrawable() const;

    virtual void fillDrawPacket(const Node& header, PartDrawPacket& packet, bool isMask) const;

    void serializeSelfImpl(::nicxlive::core::serde::TypedJsonWriter& serializer, bool recursive = true,
        SerializeNodeFlags flags = SerializeNodeFlags::All) const override;
    ::nicxlive::core::serde::ReadError deserializeFromFghj(::nicxlive::core::serde::TypedJsonView data) override;
    void serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const override;
    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;

    void runBeginTask(core::RenderContext& ctx) override;

    void runPostTaskImpl(std::size_t priority, core::RenderContext& ctx) override;

    // Source const index slice retains its backing through virtual callbacks.
    virtual common::SharedArray<std::ptrdiff_t> updatedCounterWeldingIndices(
        std::shared_ptr<Drawable> target, common::SharedArray<std::ptrdiff_t> indices);

    // Retained Source slices preserve mutable-array identity across callbacks.
    virtual common::SharedArray<std::ptrdiff_t> counterWeldingIndices(
        std::shared_ptr<Drawable> target, common::SharedArray<std::ptrdiff_t> indices);
    virtual void addWeldedTarget(std::shared_ptr<Drawable> target,
        common::SharedArray<std::ptrdiff_t> indices, float weight);

    using Deformable::rebuffer;
    virtual void rebuffer(const MeshData& data);

    // Source parameters retain descriptors and the mutable matrix through virtual dispatch.
    // Appended after all existing virtual entries, including the arrived rebuffer slot.
    virtual Node::DeformFilterResult weldingProcessor(std::shared_ptr<Node> target,
        Vec2Array originalVertices, Vec2Array originalDeformation, Mat4* originalTransform);

    void copyFrom(const Node& src, bool clone = false, bool deepCopy = true) override;

protected:
    explicit Drawable(DeferredConstruction) : Deformable(DeferredConstruction{}) {}
    void constructDrawableOwned(std::shared_ptr<Node> parent);
    void constructDrawableDataOwned(MeshData data, std::uint32_t id, std::shared_ptr<Node> parent);

private:
    Vec2Array gatherVec2(Vec2Array source, std::span<const std::size_t> indices);
    Vec2Array makeZeroVecArray(std::size_t count);
    void scatterAddVec2(Vec2Array delta, std::span<const std::size_t> indices,
        Vec2Array& target, bool& changed);
    bool updateAttachedNodeTransform(const std::shared_ptr<Node>& node);
    void registerWeldFilter(const std::shared_ptr<Drawable>& target);
    void unregisterWeldFilter(const std::shared_ptr<Drawable>& target);
};

} // namespace nicxlive::core::nodes

namespace nicxlive::core::serde {
inline ReadError deserializeValue(TypedJsonView data, nodes::WeldingLink& value) {
    return value.deserializeFromFghj(data);
}
} // namespace nicxlive::core::serde
