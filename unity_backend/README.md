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

## Unity への導入（自動セットアップ）

`install-unity-plugin.ps1` を使うと、`nicxlive.dll` のビルドから Unity プロジェクトへの配置、さらに Unity 設定の点検・差分更新までを一括で行えます。

### 実行例（通常）

```powershell
cd nicxlive/unity_backend
.\install-unity-plugin.ps1 -UnityProjectPath "C:\path\to\YourUnityProject"
```

### 実行例（Unity 実行ファイルを明示）

```powershell
.\install-unity-plugin.ps1 `
  -UnityProjectPath "C:\path\to\YourUnityProject" `
  -UnityExe "C:\Program Files\Unity\Hub\Editor\2022.3.22f1\Editor\Unity.exe"
```

### 主なオプション

- `-Config Debug|RelWithDebInfo|Release`
  - Native DLL のビルド構成を指定（既定: `RelWithDebInfo`）
- `-SkipNativeBuild`
  - `nicxlive.dll` のビルドをスキップして、既存成果物のみコピー
- `-SkipUnitySetup`
  - Unity バッチ実行による設定確認・更新をスキップ
- `-UnityExe <path>`
  - 使用する `Unity.exe` を明示指定（未指定時は一般的なパスを自動探索）
- `-CMakeExe <path>`
  - 使用する `cmake` 実行ファイルを指定

### スクリプトが行う配置

- `Assets/Plugins/x86_64/nicxlive.dll`
- `Assets/Nicxlive/Runtime/unity_backend.cs`
- `Assets/Nicxlive/Editor/NicxliveBehaviourEditor.cs`
- `Assets/Nicxlive/Editor/NicxliveProjectSetup.cs`

### 自動設定（状態確認して必要時のみ更新）

`NicxliveProjectSetup.ApplyFromCommandLine` を `Unity -batchmode` で実行し、次を確認・更新します。

1. `PlayerSettings.allowUnsafeCode == true`
2. `GraphicsSettings.currentRenderPipeline` が URP Asset
3. `QualitySettings` の各レベルで URP Asset を使用
4. `Assets/Plugins/x86_64/nicxlive.dll` の `PluginImporter` 設定（Editor/Windows x64 有効）

### Unity 側で最後に行うこと

1. `NicxliveBehaviour` を GameObject に追加
2. `PartMaterial` / `MaskMaterial` を設定

## 備考

- 実装は `nijiv` の OpenGL/DirectX バックエンドのコマンド順序と状態遷移を合わせています。
- Unity 固有制約（RenderTarget 復帰先、Blend 実機差分）は managed 側で吸収する構成です。
