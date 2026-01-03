#pragma once

#include "deformable.hpp"
#include "filter.hpp"
#include "../common/utils.hpp"

#include <algorithm>
#include <unordered_set>
#include <optional>

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

struct MeshData {
    std::vector<Vec2> vertices{};
    std::vector<Vec2> uvs{};
    std::vector<uint16_t> indices{};
    Vec2 origin{};
    std::vector<std::vector<float>> gridAxes{};

    bool isReady() const;
};

static constexpr std::ptrdiff_t NOINDEX = static_cast<std::ptrdiff_t>(-1);

struct WeldingLink {
    NodeId target{};
    std::vector<std::ptrdiff_t> indices{};
    float weight{0.0f};
};

class Drawable : public Deformable {
public:
    MeshData mesh{};
    std::vector<Vec2> deformationOffsets{};
    uint32_t vertexOffset{0};
    uint32_t uvOffset{0};
    uint32_t deformOffset{0};
    Vec3 tint{};
    Vec3 screenTint{};
    float emissionStrength{0.0f};
    std::vector<int32_t> textures{};
    std::optional<std::array<float, 4>> bounds{};

    std::vector<NodeId> weldedTargets{};
    bool welded{false};
    std::vector<WeldingLink> weldedLinks{};
    std::unordered_set<NodeId> weldingApplied{};

    Drawable() = default;

    virtual void updateIndices();

    virtual void updateVertices() override;

    virtual void updateBounds() override;

    virtual void drawMeshLines() const;
    virtual void drawMeshPoints() const;
    virtual void getMesh();

    virtual std::tuple<Vec2Array, std::optional<Mat4>, bool> nodeAttachProcessor(const std::shared_ptr<Node>& node,
                                                                                  const Vec2Array& origVertices,
                                                                                  const Vec2Array& origDeformation,
                                                                                  const Mat4* origTransform);

    virtual std::tuple<Vec2Array, std::optional<Mat4>, bool> weldingProcessor(const std::shared_ptr<Node>& target,
                                                                              const Vec2Array& origVertices,
                                                                              const Vec2Array& origDeformation,
                                                                              const Mat4* origTransform);

    virtual void rebufferMesh();

    virtual void reset();

    virtual void addWeldedTarget(const std::shared_ptr<Drawable>& target,
                                 const std::vector<std::ptrdiff_t>& indices,
                                 float weight);

    virtual void removeWeldedTarget(const std::shared_ptr<Drawable>& target);

    virtual bool isWeldedBy(NodeId target) const;

    virtual void setupSelf() override;

    virtual void finalizeDrawable();

    virtual void normalizeUv();

    virtual void clearCache() override;

    virtual void centralizeDrawable();

    virtual void copyFromDrawable(const Drawable& src);

    virtual void setupChildDrawable();
    virtual void releaseChildDrawable();

    virtual void buildDrawable(bool force);

    virtual bool mustPropagateDrawable() const;

    virtual void fillDrawPacket(const Node& header, PartDrawPacket& packet, bool isMask) const;

    void serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const override;
    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;

    void runBeginTask(core::RenderContext& ctx) override;

    void runPostTaskImpl(std::size_t priority, core::RenderContext& ctx) override;

private:
    void registerWeldFilter(const std::shared_ptr<Drawable>& target);
    void unregisterWeldFilter(const std::shared_ptr<Drawable>& target);
    void writeSharedBuffers();
};

} // namespace nicxlive::core::nodes
