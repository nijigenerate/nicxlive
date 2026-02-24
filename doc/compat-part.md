# Part 実装互換性チェック (D ↔ C++)

判定基準: D実装を正とし、Dに存在してC++にない項目は `✗（未実装）`、Dに存在せずC++のみにある項目は `✗（削除候補）` とする。

| クラス/メソッド | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| フィールド `textures` | Texture[COUNT] 実体 | std::array<Texture> 実体＋runtimeUUID同期・dirty管理 | ◯ |
| フィールド `textureIds` | テクスチャID配列 | 同等（puppetで解決） | ◯ |
| フィールド `masks` | MaskBinding 配列 | `masked_by`/`masks` の両入力を `MaskBinding` に統合して復元 | ◯ |
| フィールド `maskBindings` | あり | `masks`に集約 | ◯ |
| フィールド `blendingMode` | BlendMode | 同等 | ◯ |
| フィールド `blendMode` | BlendMode | 同等 | ◯ |
| `deserializeFromFghj` (`blend_mode`) | 文字列/数値の両方を受理 | 文字列優先で復元し、数値も受理 | ◯ |
| `deserializeFromFghj` (`mask_mode`, `masks[].mode`) | 文字列/数値の両方を受理 | 文字列/数値の両方を受理（`Mask`/`DodgeMask`） | ◯ |
| `deserializeFromFghj` (`masked_by`) | 単一 mode で mask UUID を復元 | `MaskBinding{maskSrcUUID, mode}` へ写経復元 | ◯ |
| `finalize` | puppet 解決で `masks[].maskSrc` を束縛 | 同等（`maskSrcUUID` を Drawable に解決） | ◯ |
| `rebuffer` | `rebufferMesh` 後に UV 更新 | `rebufferMesh(data)` + `updateUVs()` | ◯ |
| `isMaskedBy` | マスク有無を判定 | UUIDベース | ◯ |
| `getMaskIdx` | マスクインデックス取得 | UUIDベース | ◯ |
| `drawOne` | 即時描画（マスク考慮） | Queue/Unityパケット発行 | ◯ |
| `drawOneImmediate` | 即時描画（マスク考慮） | Queue/Unityパケット発行 | ◯ |
| `setOffscreenModelMatrix` | オフスクリーン行列設定 | 実装 | ◯ |
| `clearOffscreenModelMatrix` | オフスクリーン行列クリア | 実装 | ◯ |
| `backendMaskCount` | マスク数取得 | 実装 | ◯ |
| `backendMasks` | マスク配列取得 | 実装 | ◯ |
