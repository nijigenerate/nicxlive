#pragma once

#include "../texture.hpp"
#include "common.hpp"

namespace nicxlive::core::render {

// Original nijilive.core.render.immediate entry points and native adapters.
void inDrawTextureAtPart(const Texture& texture, const std::shared_ptr<nodes::Part>& part);
void inDrawTextureAtPosition(const Texture& texture, const nodes::Vec2& position, float opacity = 1.0f,
                             const nodes::Vec3& color = nodes::Vec3{1, 1, 1}, const nodes::Vec3& screenColor = nodes::Vec3{0, 0, 0});
void inDrawTextureAtRect(const Texture& texture, const Rect& area, const Rect& uvs = Rect{0, 0, 1, 1},
                         float opacity = 1.0f, const nodes::Vec3& color = nodes::Vec3{1, 1, 1},
                         const nodes::Vec3& screenColor = nodes::Vec3{0, 0, 0}, void* shader = nullptr, void* camera = nullptr);

// Mutable Source overloads; preserve the old const declarations and their bodies.
void inDrawTextureAtPart(Texture& texture, const std::shared_ptr<nodes::Part>& part);
void inDrawTextureAtPosition(Texture& texture, const nodes::Vec2& position, float opacity = 1.0f,
                             const nodes::Vec3& color = nodes::Vec3{1, 1, 1},
                             const nodes::Vec3& screenColor = nodes::Vec3{0, 0, 0});
void inDrawTextureAtRect(Texture& texture, const Rect& area, const Rect& uvs = Rect{0, 0, 1, 1},
                         float opacity = 1.0f, const nodes::Vec3& color = nodes::Vec3{1, 1, 1},
                         const nodes::Vec3& screenColor = nodes::Vec3{0, 0, 0},
                         void* shader = nullptr, void* camera = nullptr);

// Source entry domain: nullable owning class handles and actual value arguments.
void inDrawTextureAtPart(std::shared_ptr<Texture> texture, std::shared_ptr<nodes::Part> part);
void inDrawTextureAtPosition(std::shared_ptr<Texture> texture, nodes::Vec2 position, float opacity = 1.0f,
                             nodes::Vec3 color = {1, 1, 1}, nodes::Vec3 screenColor = {0, 0, 0});
void inDrawTextureAtRect(std::shared_ptr<Texture> texture, Rect area, Rect uvs = {0, 0, 1, 1},
                         float opacity = 1.0f, nodes::Vec3 color = {1, 1, 1}, nodes::Vec3 screenColor = {0, 0, 0},
                         std::shared_ptr<Shader> shader = {}, std::shared_ptr<math::Camera> camera = {});

} // namespace nicxlive::core::render
