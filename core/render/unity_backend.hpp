#pragma once

#include "common.hpp"

#include <vector>
#include <map>

namespace nicxlive::core::render {

// Simple Unity-compatible backend that records packets for external consumption.
class UnityQueueBackend : public UnityRenderBackend {
public:
    void submitPacket(const nodes::PartDrawPacket& pkt) override { drawPackets_.push_back(pkt); }
    void submitMask(const nodes::PartDrawPacket& pkt) override { maskPackets_.push_back(pkt); }
    void clear() override { drawPackets_.clear(); maskPackets_.clear(); }
    const std::vector<nodes::PartDrawPacket>& drawPackets() const override { return drawPackets_; }
    const std::vector<nodes::PartDrawPacket>& maskPackets() const override { return maskPackets_; }
    void initializeDrawableResources() override {}
    void bindDrawableVao() override {}
    void createDrawableBuffers(RenderResourceHandle& outHandle) override { outHandle = ++nextIbo_; }
    void uploadDrawableIndices(RenderResourceHandle id, const std::vector<uint16_t>& indices) override { iboMap_[id] = indices; }
    void uploadSharedVertexBuffer(const ::nicxlive::core::common::Vec2Array& v) override { sharedVertices_ = v; }
    void uploadSharedUvBuffer(const ::nicxlive::core::common::Vec2Array& v) override { sharedUvs_ = v; }
    void uploadSharedDeformBuffer(const ::nicxlive::core::common::Vec2Array& v) override { sharedDeform_ = v; }
    void drawDrawableElements(RenderResourceHandle id, std::size_t count) override { lastDraw_[id] = count; }
    void setDebugPointSize(float v) override { debugPointSize_ = v; }
    void setDebugLineWidth(float v) override { debugLineWidth_ = v; }
    void uploadDebugBuffer(const std::vector<nodes::Vec3>& positions, const std::vector<uint16_t>& indices) override {
        debugPositions_ = positions;
        debugIndices_ = indices;
    }
    void drawDebugPoints(const nodes::Vec4&, const nodes::Mat4&) override {}
    void drawDebugLines(const nodes::Vec4&, const nodes::Mat4&) override {}

private:
    std::vector<nodes::PartDrawPacket> drawPackets_{};
    std::vector<nodes::PartDrawPacket> maskPackets_{};
    RenderResourceHandle nextIbo_{0};
    std::map<RenderResourceHandle, std::vector<uint16_t>> iboMap_{};
    ::nicxlive::core::common::Vec2Array sharedVertices_{};
    ::nicxlive::core::common::Vec2Array sharedUvs_{};
    ::nicxlive::core::common::Vec2Array sharedDeform_{};
    std::vector<nodes::Vec3> debugPositions_{};
    std::vector<uint16_t> debugIndices_{};
    std::map<RenderResourceHandle, std::size_t> lastDraw_{};
    float debugPointSize_{1.0f};
    float debugLineWidth_{1.0f};
};

} // namespace nicxlive::core::render
