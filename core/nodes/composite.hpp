#pragma once

#include "projectable.hpp"

namespace nicxlive::core::nodes {

struct OffscreenState {
    bool enabled{false};
};

class Composite : public Projectable {
public:
    bool propagateMeshGroup{true};
    OffscreenState offscreen{};

    Composite() = default;
    explicit Composite(const MeshData& data, uint32_t uuidVal = 0);

    const std::string& typeId() const override;

    bool propagateMeshGroupEnabled() const;
    void setPropagateMeshGroup(bool value);

    float threshold() const;
    void setThreshold(float value);

    Transform transform() const;
    bool mustPropagate() const override;
    void serializeSelfImpl(::nicxlive::core::serde::InochiSerializer& serializer, bool recursive, SerializeNodeFlags flags) const override;
    ::nicxlive::core::serde::SerdeException deserializeFromFghj(const ::nicxlive::core::serde::Fghj& data) override;
    Vec4 getChildrenBounds(bool forceUpdate = true) override;
    void enableMaxChildrenBounds(const std::shared_ptr<Node>& target = nullptr) override;

private:
    Mat4 childOffscreenMatrix(const std::shared_ptr<Part>& child) const;
    Mat4 childCoreMatrix(const std::shared_ptr<Part>& child) const;
    Vec4 localBoundsFromMatrix(const std::shared_ptr<Part>& child, const Mat4& matrix) const;
};

} // namespace nicxlive::core::nodes
