#pragma once

#include "composite.hpp"

namespace nicxlive::core::nodes {

class DynamicComposite : public Projectable {
public:
    bool needsRedraw{false};
    bool cached{false};
    OffscreenState offscreen{};

    DynamicComposite() = default;
    explicit DynamicComposite(const MeshData& data, uint32_t uuidVal = 0);

    const std::string& typeId() const override;
};

} // namespace nicxlive::core::nodes
