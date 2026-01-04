#pragma once

#include "render/common.hpp"
#include "render/commands.hpp"
#include "render/backend_queue.hpp"
#include "render/command_emitter.hpp"
#include "render/graph_builder.hpp"
#include "render/scheduler.hpp"
#include "puppet.hpp"
#include "../fmt/fmt.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using MaskDrawableKind = nicxlive::core::RenderBackend::MaskDrawableKind;

extern "C" {

enum class NjgResult : int {
    Ok = 0,
    InvalidArgument = 1,
    Failure = 2,
};

enum class NjgRenderCommandKind : uint32_t {
    DrawPart,
    BeginDynamicComposite,
    EndDynamicComposite,
    BeginMask,
    ApplyMask,
    BeginMaskContent,
    EndMask,
};

struct UnityRendererConfig {
    int viewportWidth;
    int viewportHeight;
};

struct FrameConfig {
    int viewportWidth;
    int viewportHeight;
};

struct PuppetParameterUpdate {
    uint32_t parameterUuid;
    nicxlive::core::nodes::Vec2 value;
};

struct NjgParameterInfo {
    uint32_t uuid;
    bool isVec2;
    nicxlive::core::nodes::Vec2 min;
    nicxlive::core::nodes::Vec2 max;
    nicxlive::core::nodes::Vec2 defaults;
    const char* name;
    size_t nameLength;
};

struct UnityResourceCallbacks {
    void* userData;
    size_t (*createTexture)(int width, int height, int channels, int mipLevels, int format, bool renderTarget, bool stencil, void* userData);
    void (*updateTexture)(size_t handle, const uint8_t* data, size_t dataLen, int width, int height, int channels, void* userData);
    void (*releaseTexture)(size_t handle, void* userData);
};

struct NjgBufferSlice {
    const float* data;
    size_t length;
};

struct SharedBufferSnapshot {
    NjgBufferSlice vertices;
    NjgBufferSlice uvs;
    NjgBufferSlice deform;
    size_t vertexCount;
    size_t uvCount;
    size_t deformCount;
};

struct TextureStats {
    size_t created;
    size_t released;
    size_t current;
};

struct CommandQueueView {
    const void* commands;
    size_t count;
};

struct NjgPartDrawPacket {
    bool isMask;
    bool renderable;
    nicxlive::core::nodes::Mat4 modelMatrix;
    nicxlive::core::nodes::Mat4 puppetMatrix;
    nicxlive::core::nodes::Vec3 clampedTint;
    nicxlive::core::nodes::Vec3 clampedScreen;
    float opacity;
    float emissionStrength;
    float maskThreshold;
    int blendingMode;
    bool useMultistageBlend;
    bool hasEmissionOrBumpmap;
    size_t textureHandles[3];
    size_t textureCount;
    nicxlive::core::nodes::Vec2 origin;
    size_t vertexOffset;
    size_t vertexAtlasStride;
    size_t uvOffset;
    size_t uvAtlasStride;
    size_t deformOffset;
    size_t deformAtlasStride;
    const uint16_t* indices;
    size_t indexCount;
    size_t vertexCount;
};

struct NjgMaskDrawPacket {
    nicxlive::core::nodes::Mat4 modelMatrix;
    nicxlive::core::nodes::Mat4 mvp;
    nicxlive::core::nodes::Vec2 origin;
    size_t vertexOffset;
    size_t vertexAtlasStride;
    size_t deformOffset;
    size_t deformAtlasStride;
    const uint16_t* indices;
    size_t indexCount;
    size_t vertexCount;
};

struct NjgDynamicCompositePass {
    size_t textures[3];
    size_t textureCount;
    size_t stencil;
    nicxlive::core::nodes::Vec2 scale;
    float rotationZ;
    size_t origBuffer;
    int origViewport[4];
};

struct NjgMaskApplyPacket {
    MaskDrawableKind kind;
    bool isDodge;
    NjgPartDrawPacket partPacket;
    NjgMaskDrawPacket maskPacket;
};

struct NjgQueuedCommand {
    NjgRenderCommandKind kind;
    NjgPartDrawPacket partPacket;
    NjgMaskApplyPacket maskApplyPacket;
    NjgDynamicCompositePass dynamicPass;
    bool usesStencil;
};

// Renderer/Puppet handles
NjgResult njgCreateRenderer(const UnityRendererConfig* config, const UnityResourceCallbacks* callbacks, void** outRenderer);
void njgDestroyRenderer(void* renderer);
NjgResult njgLoadPuppet(void* renderer, const char* pathUtf8, void** outPuppet);
NjgResult njgLoadPuppetFromMemory(void* renderer, const uint8_t* data, size_t length, void** outPuppet);
NjgResult njgWritePuppetToMemory(void* puppet, const uint8_t** outData, size_t* outLength);
void njgFreeBuffer(const void* buffer);
NjgResult njgUnloadPuppet(void* renderer, void* puppet);
NjgResult njgGetParameters(void* puppet, NjgParameterInfo* buffer, size_t bufferLength, size_t* outCount);
NjgResult njgUpdateParameters(void* puppet, const PuppetParameterUpdate* updates, size_t updateCount);
NjgResult njgBeginFrame(void* renderer, const FrameConfig* cfg);
NjgResult njgTickPuppet(void* puppet, float deltaSeconds);
NjgResult njgEmitCommands(void* renderer, SharedBufferSnapshot* shared, const CommandQueueView** outView);
void njgFlushCommandBuffer(void* renderer);
size_t njgGetGcHeapSize();
TextureStats njgGetTextureStats(void* renderer);

} // extern "C"
