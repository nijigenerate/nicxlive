# unity_backend.cs Unity Plugin 実装再評価

対象: `nicxlive/unity_backend/unity_backend.cs`, `nicxlive/unity_backend/NicxliveBehaviourEditor.cs`  
再評価日: 2026-03-05

## 結論
- Unityプラグインとしての実行経路（`NicxliveRenderer` + `NicxliveBehaviour` + `CommandExecutor`）は実装済み。
- 前回 `Partial` とした項目は `tasks.md` の `UBP011`〜`UBP014` に従って実装し、再評価で `Partial` は 0 件。
- 追加観点は `tasks.md` の `UPE001`〜`UPE006` を実装し、再評価で `NG/内容NG` は 0 件。

## 主要項目の判定
| 項目 | 判定 | 根拠 |
|---|---|---|
| ネイティブ連携（P/Invoke + ABI検証） | OK | `unity_backend.cs:18`, `unity_backend.cs:343` |
| SharedBuffer/CommandQueue 取り込み | OK | `unity_backend.cs:433`, `unity_backend.cs:476` |
| Texture/Shader 管理 | OK | `unity_backend.cs:530`, `unity_backend.cs:762` |
| URP描画フック（beginCameraRendering） | OK | `unity_backend.cs:1080`, `unity_backend.cs:1127` |
| Render command 実行（Part/Mask/DynamicComposite） | OK | `unity_backend.cs:2872` |
| `createShader(string,string)` のソース反映 | OK | `unity_backend.cs:1762`, `unity_backend.cs:1853`, `unity_backend.cs:1884` |
| Present program 実体化 | OK | `unity_backend.cs:2983`, `unity_backend.cs:3070` |
| 高度ブレンド能力判定/初期化 | OK | `unity_backend.cs:1437`, `unity_backend.cs:1442`, `unity_backend.cs:1838` |
| DirectX互換ランタイム層 | OK | `unity_backend.cs:2318`, `unity_backend.cs:2352`, `unity_backend.cs:2358`, `unity_backend.cs:2479`, `unity_backend.cs:2517` |
| スタブだった互換API群（`dlopen`/`dlsym`/`sampleTex` 等） | OK | `unity_backend.cs:2040`, `unity_backend.cs:2061`, `unity_backend.cs:2183` |

## スタブ解消確認
| 旧スタブ項目 | 判定 | 根拠 |
|---|---|---|
| `drawDrawableElements` | OK | `unity_backend.cs:1559` |
| `offscreenRtvHandleAt` | OK | `unity_backend.cs:1571` |
| `enqueueSpan` | OK | `unity_backend.cs:1586` |
| `drawUploadedGeometry` | OK | `unity_backend.cs:1595` |
| `RenderingBackend.issueBlendBarrier` | OK | `unity_backend.cs:1430` |
| `glGetShaderiv` / `glGetProgramiv` / `glDeleteShader` | OK | `unity_backend.cs:1972`, `unity_backend.cs:2011`, `unity_backend.cs:2001` |
| `SDL_GL_SetAttribute` / `queryWindowPixelSize` / `requireWindowHandle` | OK | `unity_backend.cs:2080`, `unity_backend.cs:2145`, `unity_backend.cs:2160` |
| `WinD3D12*` / `D3D12_ROOT_DESCRIPTOR_TABLE` | OK | `unity_backend.cs:2240`, `unity_backend.cs:2252` |
| `DirectXRuntime.ensureUploadBuffer` | OK | `unity_backend.cs:2403` |

## 最終判定
- `Partial`: 0
- `NG`: 0
- Unityプラグイン互換実装として、現行 `unity_backend.cs` は評価基準上 `OK`。

## 追加観点評価（Editor運用）
| 観点 | 判定 | 根拠 |
|---|---|---|
| Runを押さずとも常に対象が表示される | OK | `NicxliveBehaviour` に `[ExecuteAlways]` を付与し、Edit Mode でも `Update` で描画を継続 (`unity_backend.cs:3938`, `unity_backend.cs:3982`, `unity_backend.cs:4023`) |
| 対象のスケールをエディタで設定できる | OK | `ModelScale` を公開し、`transform.lossyScale` と合成して `njgSetPuppetScale` へ反映 (`unity_backend.cs:3943`, `unity_backend.cs:4251`, `unity_backend.cs:4263`) |
| GameObjectの位置にモデルを表示できる | OK | `transform.position` をスクリーン座標へ変換し、`njgSetPuppetTranslation` に反映 (`unity_backend.cs:4247`, `unity_backend.cs:4248`, `unity_backend.cs:4264`) |
| モデルファイル/リソースをEditorから選択できる | OK | `PuppetAsset` を追加し、CustomEditor で Asset 選択 + `PuppetPath` 反映を実装 (`unity_backend.cs:3941`, `NicxliveBehaviourEditor.cs:9`, `NicxliveBehaviourEditor.cs:49`) |
| モデルパラメータを操作する機能がある | OK | `njgGetParameters`/`njgUpdateParameters` を managed 実装し、CustomEditor でスライダー編集を実装 (`unity_backend.cs:339`, `unity_backend.cs:2820`, `unity_backend.cs:4078`, `NicxliveBehaviourEditor.cs:125`, `NicxliveBehaviourEditor.cs:163`) |
| Run時に物理演算などが動く | OK | Play中は `Time.deltaTime` を `Tick` に渡して更新し、Run時のフレーム進行を維持 (`unity_backend.cs:4015`, `unity_backend.cs:4269`) |
