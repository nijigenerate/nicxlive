#pragma once

#include "../nodes/common.hpp"
#include <vector>

namespace nicxlive::core::nodes {

struct PartDrawPacket {
    Mat4 modelMatrix{Mat4::identity()};
    Mat4 puppetMatrix{Mat4::identity()};
    bool renderable{false};
    bool isMask{false};
    bool ignorePuppet{false};
    float opacity{1.0f};
    float emissionStrength{1.0f};
    BlendMode blendMode{BlendMode::Normal};
    bool useMultistageBlend{false};
    bool hasEmissionOrBumpmap{false};
    float maskThreshold{0.0f};
    Vec3 clampedTint{1.0f, 1.0f, 1.0f};
    Vec3 clampedScreen{0.0f, 0.0f, 0.0f};
    Vec2 origin{};
    uint32_t vertexOffset{0};
    uint32_t uvOffset{0};
    uint32_t deformOffset{0};
    uint32_t vertexAtlasStride{0};
    uint32_t uvAtlasStride{0};
    uint32_t deformAtlasStride{0};
    uint32_t vertexCount{0};
    uint32_t indexCount{0};
    uint32_t textureUUIDs[3]{};
    std::vector<uint16_t> indices{};
    std::vector<Vec2> vertices{};
    std::vector<Vec2> uvs{};
    ::nicxlive::core::common::Vec2Array deformation{};
    std::weak_ptr<class Part> node{};
};

} // namespace nicxlive::core::nodes
