#pragma once

#include "../texture.hpp"
#include "common.hpp"

namespace nicxlive::core::render {

// D: nijilive.core.render.immediate に相当する即時描画ユーティリティ
void inDrawTextureAtPart(const Texture& texture, const std::shared_ptr<nodes::Part>& part);
void inDrawTextureAtPosition(const Texture& texture, const nodes::Vec2& position, float opacity = 1.0f,
                             const nodes::Vec3& color = nodes::Vec3{1, 1, 1}, const nodes::Vec3& screenColor = nodes::Vec3{0, 0, 0});
void inDrawTextureAtRect(const Texture& texture, const Rect& area, const Rect& uvs = Rect{0, 0, 1, 1},
                         float opacity = 1.0f, const nodes::Vec3& color = nodes::Vec3{1, 1, 1},
                         const nodes::Vec3& screenColor = nodes::Vec3{0, 0, 0}, void* shader = nullptr, void* camera = nullptr);

} // namespace nicxlive::core::render

