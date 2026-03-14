# nicxlive unity_backend

This directory contains the Unity managed backend that mirrors the rendering flow of the OpenGL and DirectX backends implemented in `nijiv/source/opengl/opengl_backend.d` and `nijiv/source/directx/directx_backend.d`.

## Overview

- `unity_backend.cs`
  - Defines the P/Invoke bindings and packet structs that must stay ABI-compatible with `nicxlive/core/unity_native.hpp`
  - Decodes `CommandQueueView` and reads `SharedBufferSnapshot` SoA buffers
  - Registers `UnityResourceCallbacks`
  - Executes `DrawPart`, `ApplyMask`, `BeginMask`, `BeginMaskContent`, `EndMask`, and `DynamicComposite`
  - Wraps renderer creation and command emission
  - Drives the Unity scene integration through `NicxliveBehaviour`

## Assumptions

- The native plugin is exposed as `nicxlive` and loaded through `DllImport("nicxlive")`
- The Unity project uses URP materials with the expected property names
  - `_MainTex`, `_EmissionTex`, `_BumpMap`
  - `_Opacity`, `_MultColor`, `_ScreenColor`, `_EmissionStrength`, `_MaskThreshold`
  - `_BlendMode`, `_StencilComp`, `_StencilRef`, `_StencilPass`, `_ColorMask`, `_IsMaskPass`

## Installing Into a Unity Project

Use `install-unity-plugin.ps1` to build `nicxlive.dll`, copy the managed runtime files into a Unity project, and optionally apply Unity project settings in batchmode.

### Build and install

```powershell
cd nicxlive/unity_backend
.\install-unity-plugin.ps1 -UnityProjectPath "C:\path\to\YourUnityProject"
```

### Build, install, and run Unity in batchmode

```powershell
.\install-unity-plugin.ps1 `
  -UnityProjectPath "C:\path\to\YourUnityProject" `
  -UnityExe "C:\Program Files\Unity\Hub\Editor\2022.3.22f1\Editor\Unity.exe"
```

### Main options

- `-Config Debug|RelWithDebInfo|Release`
  - Selects the native DLL build configuration. Default is `RelWithDebInfo`.
- `-SkipNativeBuild`
  - Skips rebuilding `nicxlive.dll` and only copies managed files.
- `-SkipUnitySetup`
  - Skips the Unity project setup pass.
- `-UnityExe <path>`
  - Explicit path to `Unity.exe`. Required only when auto-detection is not enough.
- `-CMakeExe <path>`
  - Explicit path to `cmake`.

### Files copied into the Unity project

- `Assets/Plugins/x86_64/nicxlive.dll`
- `Assets/Nicxlive/Runtime/unity_backend.cs`
- `Assets/Nicxlive/Runtime/NicxlivePartURP.shader`
- `Assets/Nicxlive/Runtime/NicxliveMaskURP.shader`
- `Assets/Nicxlive/Runtime/NicxlivePresentURP.shader`
- `Assets/Nicxlive/Editor/NicxliveBehaviourEditor.cs`
- `Assets/Nicxlive/Editor/NicxliveProjectSetup.cs`

### Automatic Unity project setup

`NicxliveProjectSetup.ApplyFromCommandLine` can be invoked from `Unity -batchmode` to apply the required project settings. The setup currently enforces:

1. `PlayerSettings.allowUnsafeCode == true`
2. `GraphicsSettings.currentRenderPipeline` is assigned to a URP asset
3. Every quality level uses the same URP asset
4. `Assets/Plugins/x86_64/nicxlive.dll` is imported for Editor / Windows x64

### After import in Unity

1. Add `NicxliveBehaviour` to a `GameObject`
2. Assign `PartMaterial` and `MaskMaterial`

## Notes

- The implementation keeps the command model close to the OpenGL and DirectX backends so behavior differences are easier to isolate.
- Render target management, blending, stencil handling, and present behavior are implemented on the managed side for Unity.
