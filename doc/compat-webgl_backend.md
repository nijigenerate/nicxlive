# WebGL Backend 実装互換性チェック (D `nijiv/source/opengl/opengl_backend.d` ↔ JS `webgl_backend/webgl_backend.js`)

判定基準: D実装を正とし、Dに存在してJSにない項目は `✗（未実装）`、Dに存在せずJSのみにある項目は `✗（削除候補）` とする。

| メソッド / 対象項目 | D 実装 | JS 現状 | 互換性評価 |
| --- | --- | --- | --- |
| `initOpenGLBackend` | SDL + GL context 初期化、callbacks生成 | `initWebGLBackend` で WebGL2 context 初期化、callbacks生成 | ◯ |
| `UnityResourceCallbacks.createTexture` | handle発行、Texture生成 | `createTexture` で handle発行、WebGL texture生成 | ◯ |
| `UnityResourceCallbacks.updateTexture` | dataLen検証、upload | `updateTexture` で dataLen検証、upload | ◯ |
| `UnityResourceCallbacks.releaseTexture` | texture破棄、dynamic FBO cache掃除 | `releaseTexture` で texture/FBO cache掃除 | ◯ |
| `renderCommands` | shared SoA upload + command playback | `renderCommands` で同順に再生 | ◯ |
| `uploadSoA`（頂点/UV/deform） | `glBufferData` で shared buffer upload | `uploadShared*Buffer` で `gl.bufferData` | ◯ |
| `beginScene/endScene` | フレームGL state管理（MRT/FBO接続、clear、blend index制御、復帰） | `fBuffer/cfBuffer` 相当ターゲットを導入し、接続・clear・`glEnablei/glDisablei`・`glDrawBuffers` 復帰を写経実装（default framebuffer の drawBuffers は WebGL2 制約に合わせ `BACK` を使用） | ◯ |
| `bindDrawableVao` | VAO bind | `bindDrawableVao` 実装 | ◯ |
| `bindPartShader` | part shader bind | `bindPartShader` 実装 | ◯ |
| `setLegacyBlendMode` | 各 BlendMode の固定関数合成設定 | D の分岐（`blendFuncSeparate` / `blendEquationSeparate` を含む）へ写経修正済み | ◯ |
| `setAdvancedBlendEquation` | KHR拡張利用 | `KHR_blend_equation_advanced(_coherent)` の拡張定数を使う経路を追加。未対応環境は D fallback と同様に legacy 経路へ退避 | ◯ |
| `supportsAdvancedBlend*` | KHR拡張検出 | extension 検出に基づく true/false を返す実装へ更新 | ◯ |
| `applyBlendMode` | legacy/advanced切替 | 同名実装（advanced可否判定 + fallback） | ◯ |
| `blendModeBarrier` / `issueBlendBarrier` | non-coherent advanced blend同期 | `blendBarrierKHR` 呼び出し経路を追加（拡張未対応時は no-op） | ◯ |
| `executeMaskPacket` | mask shader + shared vertex/deform | 同名実装 | ◯ |
| `beginMask` | stencil write phase開始 | 同名実装 | ◯ |
| `beginMaskContent` | stencil test phase開始 | 同名実装 | ◯ |
| `endMask` | mask phase終了 | 同名実装 | ◯ |
| `applyMask` | `MaskApplyPacket` に従って mask+content 描画 | 同名実装 | ◯ |
| `setupShaderStage` | stage別 uniform/blend設定 | `partShaderStage1/2/3` と stage別 `drawBuffers` を実装 | ◯ |
| `renderStage` | SoA attrib bind + draw + barrier | 同名実装 | ◯ |
| `drawPartPacket` | packet検証・texture bind・draw | 同名実装 | ◯ |
| `drawDrawableElements` | IBO + index count で描画 | 同名実装 | ◯ |
| `getOrCreateIboByHandle` | `indexHandle` 単位IBO管理 | 同名実装 | ◯ |
| `createDynamicCompositePass` | surface/pass組み立て + FBO解決 | 実装済み（surface/FBO解決、stencil接続、pass情報構築） | ◯ |
| `beginDynamicComposite` | offscreen FBOへ遷移、clear | 実装済み（`origBuffer/origViewport` 更新、viewport stack push、drawBuffers 設定、stencil/color clear順をDに整合） | ◯ |
| `endDynamicComposite` | 元FBO/viewport復帰 | 実装済み（`rebindActiveTargets` 後に `DRAW/READ_FRAMEBUFFER` を `origBuffer` へ復帰、viewport復帰） | ◯ |
| `postProcessScene` | 最終ポストプロセス | `fBuffer/cfBuffer` を使う ping-pong と最終 `blitFramebuffer`、`area` 頂点更新、`ambientLight`/`fbSize`/`mvp` uniform 供給を写経実装 | ◯ |
| `presentSceneToBackbuffer` | backbufferへ提示 | present shader + full-screen quad + `srcTex/useColorKey` を実装し、入力は `fAlbedo` 固定へ変更 | ◯ |
| shader stage分離 (`partShaderStage1/2/3`) | 3段シェーダ構成 | `partShaderStage1/2/3` を分離実装 | ◯ |
| MRT (`drawBuffers` 制御) | stage毎にattachment切替 | `setDrawBuffersSafe` と stage別切替、`beginScene/endScene` の `enablei/disablei` を実装 | ◯ |
| dynamic composite stencil attachment | stencil texture付きFBO | `createTexture(stencil=true)` で `DEPTH24_STENCIL8` を生成し、`createDynamicCompositePass` で `STENCIL_ATTACHMENT` 接続 | ◯ |
| texture parameter APIs (`setFiltering`等) | backend経由で詳細設定 | `bindTextureHandle` / `uploadTextureData` / `generateTextureMipmap` / `applyTextureFiltering` / `applyTextureWrapping` / `applyTextureAnisotropy` / `readTextureData` を実装。`EXT_texture_border_clamp` 対応環境では `ClampToBorder` を反映し、未対応時は `CLAMP_TO_EDGE` フォールバック。`readTextureData` は `readPixels` 経由実装 | △ |
| `rebindActiveTargets` | 現在の render target 再接続 | `fBuffer/cfBuffer` の attachment 再接続を実装 | ◯ |
| `framebufferHandle` | backend の framebuffer handle 取得 | `fBuffer` を返す実装へ修正 | ◯ |
| Shader handle API (`useShader` 等) | shader handle の作成/破棄/uniform 設定 | `createShader/useShader/destroyShader/getShaderUniformLocation/setShaderUniform` と型別ラッパ（bool/int/float/vec/mat4）を実装 | ◯ |
| Viewport stack (`pushViewport/popViewport/getViewport`) | viewport 保存復帰 | 実装追加（width/height stack を保持） | ◯ |
| debug mesh/thumbnail path | debug points/lines/thumbnail描画 | `uploadDebugBuffer` / `drawDebugPoints` / `drawDebugLines` / `renderThumbnailGrid` 実装 | ◯ |

## 補足
- 本ファイルは `opengl_backend.d` のコマンド再生系を最優先で写経した差分表であり、SDL/プラットフォーム分岐・高度な OpenGL 拡張は段階的に追従する。
- 進捗は `doc/tasks.md` の `CP150+` で管理する。
