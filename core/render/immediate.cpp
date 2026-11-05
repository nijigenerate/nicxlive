#include "immediate.hpp"
#include "common.hpp"
#include "../runtime_state.hpp"

namespace nicxlive::core::render {

void inDrawTextureAtPart(const Texture& texture, const std::shared_ptr<nodes::Part>& part) {
    if (!part) return;
    if (auto backend = std::dynamic_pointer_cast<RenderBackend>(getCurrentRenderBackend())) {
        backend->drawTextureAtPart(texture, part);
    }
}

void inDrawTextureAtPosition(const Texture& texture, const nodes::Vec2& position, float opacity,
                             const nodes::Vec3& color, const nodes::Vec3& screenColor) {
    if (auto backend = std::dynamic_pointer_cast<RenderBackend>(getCurrentRenderBackend())) {
        backend->drawTextureAtPosition(texture, position, opacity, color, screenColor);
    }
}

void inDrawTextureAtRect(const Texture& texture, const Rect& area, const Rect& uvs, float opacity,
                         const nodes::Vec3& color, const nodes::Vec3& screenColor, void* shader, void* camera) {
    if (auto backend = std::dynamic_pointer_cast<RenderBackend>(getCurrentRenderBackend())) {
        backend->drawTextureAtRect(texture, area, uvs, opacity, color, screenColor, shader, camera);
    }
}

void inDrawTextureAtPart(Texture& texture, const std::shared_ptr<nodes::Part>& part) {
    if (!part) return;
    if (auto backend = std::dynamic_pointer_cast<RenderBackend>(getCurrentRenderBackend())) {
        backend->drawTextureAtPart(texture, part);
    }
}
void inDrawTextureAtPosition(Texture& texture, const nodes::Vec2& position, float opacity,
                             const nodes::Vec3& color, const nodes::Vec3& screenColor) {
    if (auto backend = std::dynamic_pointer_cast<RenderBackend>(getCurrentRenderBackend())) {
        backend->drawTextureAtPosition(texture, position, opacity, color, screenColor);
    }
}
void inDrawTextureAtRect(Texture& texture, const Rect& area, const Rect& uvs, float opacity,
                         const nodes::Vec3& color, const nodes::Vec3& screenColor, void* shader, void* camera) {
    if (auto backend = std::dynamic_pointer_cast<RenderBackend>(getCurrentRenderBackend())) {
        backend->drawTextureAtRect(texture, area, uvs, opacity, color, screenColor, shader, camera);
    }
}

void inDrawTextureAtPart(std::shared_ptr<Texture> texture, std::shared_ptr<nodes::Part> part) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
    if (!texture || !part) return;
    const auto backend = currentRenderBackend();
    if (!backend) return;
    backend->drawTextureAtPart(texture, part);
#endif
}
void inDrawTextureAtPosition(std::shared_ptr<Texture> texture, nodes::Vec2 position, float opacity,
                             nodes::Vec3 color, nodes::Vec3 screenColor) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
    if (!texture) return;
    const auto backend = currentRenderBackend();
    if (!backend) return;
    backend->drawTextureAtPosition(texture, position, opacity, color, screenColor);
#endif
}
void inDrawTextureAtRect(std::shared_ptr<Texture> texture, Rect area, Rect uvs, float opacity,
                         nodes::Vec3 color, nodes::Vec3 screenColor,
                         std::shared_ptr<Shader> shader, std::shared_ptr<math::Camera> camera) {
#ifndef NICXLIVE_SOURCE_NO_RENDER
    if (!texture) return;
    const auto backend = currentRenderBackend();
    if (!backend) return;
    backend->drawTextureAtRect(texture, area, uvs, opacity, color, screenColor, shader, camera);
#endif
}

} // namespace nicxlive::core::render
