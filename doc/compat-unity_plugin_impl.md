# Unity Backend Compatibility Surface

This note describes the compatibility surface that is still intentionally kept in `nicxlive/unity_backend/unity_backend.cs`.
It replaces the older document that listed removed SDL/OpenGL/DirectX shim functions as if they were still active.

## Current State

The managed Unity backend no longer carries the old generic backend emulation layer.
The active runtime path is:

- `Nicxlive.UnityBackend.Interop.NicxliveNative`
- `Nicxlive.UnityBackend.Managed.NicxliveRenderer`
- `Nicxlive.UnityBackend.Managed.CommandExecutor`
- `Nicxlive.UnityBackend.Managed.NicxliveBehaviour`

The only compatibility helpers intentionally retained under `Nicxlive.UnityBackend.Compat` are:

- `UrpShaderCatalog`
- `UrpCommandBufferRouter`

These are still used by the current runtime implementation.

## Kept Compatibility Helpers

| Helper | Purpose | Still used by |
|---|---|---|
| `UrpShaderCatalog` | Resolves required URP shaders for part and mask rendering. | `TextureRegistry`, `NicxliveBehaviour`, material setup paths |
| `UrpCommandBufferRouter` | Attaches and detaches URP command buffers and records execution diagnostics. | `NicxliveBehaviour` camera routing and diagnostics |

## Removed Legacy Compatibility Layer

The following categories were removed from `unity_backend.cs` because they are not part of the current Unity runtime path:

- SDL-style window and GL bootstrap helpers
- OpenGL backend init structs and fake runtime wrappers
- DirectX backend init structs and fake runtime wrappers
- Generic backend registry / backend switching layer
- Shader handle emulation helpers
- Texture handle emulation helpers that existed only for the removed backend shim
- Stubbed platform bridge functions that were only kept for historical compatibility

In practical terms, the Unity managed backend no longer defines or uses legacy items such as:

- `BackendRegistry`
- `RenderingBackend`
- `DirectXRuntime`
- `OpenGLBackendInit(...)`
- `DirectXBackendInit(...)`
- shader-handle wrapper types
- texture-handle wrapper types
- old `createShader(...)` / `useShader(...)` / `destroyShader(...)` shim API
- old `createTextureHandle(...)` / `bindTextureHandle(...)` / `uploadTextureData(...)` shim API

## Why This Was Removed

Those functions were not part of the active Unity execution path and made the file harder to reason about.
Keeping them suggested that the Unity backend was still maintaining a cross-backend emulation layer, which is no longer true.
The runtime now relies on the native ABI plus the URP-specific managed path only.

## Maintenance Rule

When adding new Unity runtime features, keep them in the active managed path.
Do not reintroduce SDL/OpenGL/DirectX compatibility shims into `unity_backend.cs` unless they are required by the current Unity runtime and are actually called.
