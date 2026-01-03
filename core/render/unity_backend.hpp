#pragma once

#include "common.hpp"

#include <vector>

namespace nicxlive::core::render {

// Simple Unity-compatible backend that records packets for external consumption.
class UnityQueueBackend : public UnityRenderBackend {
public:
    void submitPacket(const nodes::PartDrawPacket& pkt) override { drawPackets_.push_back(pkt); }
    void submitMask(const nodes::PartDrawPacket& pkt) override { maskPackets_.push_back(pkt); }
    void clear() override { drawPackets_.clear(); maskPackets_.clear(); }
    const std::vector<nodes::PartDrawPacket>& drawPackets() const override { return drawPackets_; }
    const std::vector<nodes::PartDrawPacket>& maskPackets() const override { return maskPackets_; }

private:
    std::vector<nodes::PartDrawPacket> drawPackets_{};
    std::vector<nodes::PartDrawPacket> maskPackets_{};
};

} // namespace nicxlive::core::render
