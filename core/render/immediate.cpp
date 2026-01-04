#include "immediate.hpp"
#include "common.hpp"

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

} // namespace nicxlive::core::render

