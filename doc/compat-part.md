# Part 実装互換性チェック (D ↔ C++)

| クラス/メソッド | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| フィールド `textures` | Texture[COUNT] 実体 | std::array<Texture> 実体＋runtimeUUID同期・dirty管理 | ◯ |
| フィールド `textureIds` | テクスチャID配列 | 同等（puppetで解決） | ◯ |
| フィールド `masks` | MaskBinding 配列 | maskList/masked_by反映済 | ◯ |
| フィールド `maskList` | MaskLink 配列 | 保存・復元 | ◯ |
| フィールド `maskBindings` | あり | `masks`に集約 | ◯ |
| フィールド `blendingMode`/`blendMode` | BlendMode | 同等 | ◯ |
| フィールド `maskAlphaThreshold` | あり | 同等 | ◯ |
| フィールド `opacity` | あり | 同等 | ◯ |
| フィールド `emissionStrength` | あり | 同等 | ◯ |
| フィールド `tint` | vec3 | 保存/復元 | ◯ |
| フィールド `screenTint` | vec3 | 保存/復元 | ◯ |
| フィールド `ignorePuppet` | あり | 同等 | ◯ |
| フィールド `offset*` | mask/opacity/tint/screenTint/emission オフセット | 追加 | ◯ |
| フィールド `offscreenModelMatrix` 等 | offscreen用行列 | 追加 | ◯ |
| コンストラクタ(MeshData, textures, uuid, parent) | タスク登録＋UV更新 | Mesh/tex受取＋タスク登録・UV更新 | ◯ |
| `initPartTasks` | RenderTask登録 | RenderTask登録 | ◯ |
| `updateUVs` | shared UV resize＋dirty | sharedUvResize/MarkDirty | ◯ |
| `drawSelf` | backendへ描画パケット | RenderGraph/Queue/Unityへパケット発行 | ◯ |
| `typeId` | "Part" | 実装 | ◯ |
| `serializeSelfImpl` | textures/masks/blend/tint/screenTint/emission 等 | tint/screenTint/ignorePuppet/textureUUID/masked_by/maskList 保存 | ◯ |
| `deserializeFromFghj` | 同等復元 | 上記全て復元＋puppetでtexture解決 | ◯ |
| `serializePartial` | textureUUIDs 部分シリアライズ | textureUUIDs のみ | ◯ |
| `renderMask` | MaskApplyPacket 生成 | backendへMaskApplyPacket発行 | ◯ |
| `hasParam` | alphaThreshold/opacity/tint/screenTint/emission 対応 | 実装 | ◯ |
| `getDefaultValue` | 上記パラメータ既定値 | 実装 | ◯ |
| `setValue` | パラメータ応じてoffset更新 | 実装 | ◯ |
| `getValue` | offset取得 | 実装 | ◯ |
| `isMaskedBy`/`getMaskIdx` | マスク有無・インデックス取得 | UUIDベース | ◯ |
| `runBeginTask` | offset初期化＋super | 実装 | ◯ |
| `rebuffer` | Mesh再設定＋UV更新 | 実装 | ◯ |
| `draw` | 描画 | パケット発行 | ◯ |
| `enqueueRenderCommands` | RenderGraphへ描画・マスク | RenderGraph/Queue/Unity対応 | ◯ |
| `runRenderTask` | enqueueRenderCommands呼び出し | 実装 | ◯ |
| `drawOne`/`drawOneImmediate` | 即時描画（マスク考慮） | Queue/Unityパケット発行 | ◯ |
| `fillDrawPacket` (Part) | opacity/tint/atlas等をパケット設定 | 詳細パケット構築（uuid/offset/stride含む） | ◯ |
| `immediateModelMatrix` | puppet/override/oneTime考慮 | offscreen/override/oneTime対応 | ◯ |
| `setOffscreenModelMatrix`/`clearOffscreenModelMatrix` | オフスクリーン行列設定 | 実装 | ◯ |
| `backendRenderable` | enabled && data.isReady | 実装 | ◯ |
| `backendMaskCount`/`backendMasks` | マスク情報取得 | 実装 | ◯ |
| `drawOneDirect` | マスク判定付き描画 | Queue/Unityへの即時パケット | ◯ |
| `finalize` | マスクリンク解決 | masked_by/maskList適用＋texture解決 | ◯ |
