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

using NjgLogFn = void (*)(const char* message, size_t length, void* userData);

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

struct NjgWasmLayout {
    uint32_t sizeQueued;
    uint32_t offQueuedPart;
    uint32_t offQueuedMaskApply;
    uint32_t offQueuedDynamic;
    uint32_t offQueuedUsesStencil;

    uint32_t sizePart;
    uint32_t offPartTextureHandles;
    uint32_t offPartTextureCount;
    uint32_t offPartOrigin;
    uint32_t offPartVertexOffset;
    uint32_t offPartVertexStride;
    uint32_t offPartUvOffset;
    uint32_t offPartUvStride;
    uint32_t offPartDeformOffset;
    uint32_t offPartDeformStride;
    uint32_t offPartIndexHandle;
    uint32_t offPartIndicesPtr;
    uint32_t offPartIndexCount;
    uint32_t offPartVertexCount;

    uint32_t sizeMaskDraw;
    uint32_t offMaskDrawIndicesPtr;
    uint32_t offMaskDrawIndexCount;
    uint32_t offMaskDrawVertexOffset;
    uint32_t offMaskDrawVertexStride;
    uint32_t offMaskDrawDeformOffset;
    uint32_t offMaskDrawDeformStride;
    uint32_t offMaskDrawIndexHandle;
    uint32_t offMaskDrawVertexCount;

    uint32_t sizeMaskApply;
    uint32_t offMaskKind;
    uint32_t offMaskIsDodge;
    uint32_t offMaskPartPacket;
    uint32_t offMaskMaskPacket;

    uint32_t sizeDynamicPass;
    uint32_t offDynTextures;
    uint32_t offDynTextureCount;
    uint32_t offDynStencil;
    uint32_t offDynScale;
    uint32_t offDynRotationZ;
    uint32_t offDynAutoScaled;
    uint32_t offDynOrigBuffer;
    uint32_t offDynOrigViewport;
    uint32_t offDynDrawBufferCount;
    uint32_t offDynHasStencil;
};

struct TextureStats {
    size_t created;
    size_t released;
    size_t current;
};

struct NjgRenderTargets {
    size_t renderFramebuffer;
    size_t compositeFramebuffer;
    size_t blendFramebuffer;
    int viewportWidth;
    int viewportHeight;
};

struct NjgQueuedCommand;

struct CommandQueueView {
    const NjgQueuedCommand* commands;
    size_t count;
};

struct NjgPartDrawPacket {
    bool isMask;
    bool renderable;
    nicxlive::core::nodes::Mat4 modelMatrix;
    nicxlive::core::nodes::Mat4 renderMatrix;
    float renderRotation;
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
    size_t indexHandle;
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
    size_t indexHandle;
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
    bool autoScaled;
    size_t origBuffer;
    int origViewport[4];
    int drawBufferCount;
    bool hasStencil;
};

enum class MaskDrawableKind : uint32_t {
    Part,
    Mask,
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
void njgRuntimeInit();
void njgRuntimeTerm();
NjgResult njgCreateRenderer(const UnityRendererConfig* config, const UnityResourceCallbacks* callbacks, void** outRenderer);
void njgDestroyRenderer(void* renderer);
NjgResult njgLoadPuppet(void* renderer, const char* pathUtf8, void** outPuppet);
NjgResult njgUnloadPuppet(void* renderer, void* puppet);
NjgResult njgGetParameters(void* puppet, NjgParameterInfo* buffer, size_t bufferLength, size_t* outCount);
NjgResult njgUpdateParameters(void* puppet, const PuppetParameterUpdate* updates, size_t updateCount);
NjgResult njgGetPuppetExtData(void* puppet, const char* key, const uint8_t** outData, size_t* outLength);
NjgResult njgPlayAnimation(void* renderer, void* puppet, const char* name, bool loop, bool playLeadOut);
NjgResult njgPauseAnimation(void* renderer, void* puppet, const char* name);
NjgResult njgStopAnimation(void* renderer, void* puppet, const char* name, bool immediate);
NjgResult njgSeekAnimation(void* renderer, void* puppet, const char* name, int frame);
NjgResult njgSetPuppetScale(void* puppet, float sx, float sy);
NjgResult njgSetPuppetTranslation(void* puppet, float tx, float ty);
NjgResult njgBeginFrame(void* renderer, const FrameConfig* cfg);
NjgResult njgTickPuppet(void* puppet, double deltaSeconds);
NjgResult njgEmitCommands(void* renderer, CommandQueueView* outView);
NjgResult njgGetSharedBuffers(void* renderer, SharedBufferSnapshot* snapshot);
NjgRenderTargets njgGetRenderTargets(void* renderer);
void njgSetLogCallback(NjgLogFn callback, void* userData);
void njgFlushCommandBuffer(void* renderer);
size_t njgGetGcHeapSize();
TextureStats njgGetTextureStats(void* renderer);
NjgResult njgGetWasmLayout(NjgWasmLayout* outLayout);

} // extern "C"
