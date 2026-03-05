# nicxlive unity_backend

`nijiv/source/opengl/opengl_backend.d` と `nijiv/source/directx/directx_backend.d` のキュー再生順を基準にした Unity managed 実装です。

## 構成

- `unity_backend.cs`
  - `nicxlive/core/unity_native.hpp` と同一 ABI の P/Invoke/struct 定義
  - `CommandQueueView` decode / `SharedBufferSnapshot` SoA コピー
  - `UnityResourceCallbacks` 実装
  - `DrawPart/Mask/DynamicComposite` の再生器
  - `njgCreateRenderer` から `njgEmitCommands` の呼び出しラッパ
  - Unity scene でのフレーム駆動 (`NicxliveBehaviour`)

## 前提

- Native DLL 名は `nicxlive`（`DllImport("nicxlive")`）
- Unity 側マテリアルで以下プロパティ名を使う前提:
  - `_MainTex`, `_EmissionTex`, `_BumpMap`
  - `_Opacity`, `_MultColor`, `_ScreenColor`, `_EmissionStrength`, `_MaskThreshold`
  - `_BlendMode`, `_StencilComp`, `_StencilRef`, `_StencilPass`, `_ColorMask`, `_IsMaskPass`

## 備考

- 実装は `nijiv` の OpenGL/DirectX バックエンドのコマンド順序と状態遷移を合わせています。
- Unity 固有制約（RenderTarget 復帰先、Blend 実機差分）は managed 側で吸収する構成です。
