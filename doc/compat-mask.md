# Mask 実装互換性チェック (D ↔ C++)

| メソッド | D 実装 | C++ 現状 | 互換性評価 |
| --- | --- | --- | --- |
| コンストラクタ群 | デフォルト／MeshData+uuid+parent オーバーロード | デフォルト／MeshData+uuid（parent引数なし） | △ |
| `typeId` | `"Mask"` を返す | 同等 | ◯ |
| `drawSelf` | 空実装 | Part の空実装を継承（同等） | ◯ |
| `fillDrawPacket` | Part処理後、`renderable=false` にし textures をクリア | `renderable=false` のみで textures 不変 | △ |
| `serializeSelfImpl` | Drawable と同等 | Drawable と同等 | ◯ |
| `deserializeFromFghj` | Drawable と同等 | Drawable と同等 | ◯ |
| `renderMask` | backend に MaskApplyPacket を発行 | 空実装（マスク適用されない） | ✗ |
| `fillMaskDrawPacket` | modelMatrix/mvp、origin、vertex/uv/deform オフセット/stride を設定 | modelMatrix と renderable=false のみ | ✗ |
| `runRenderTask` | 空実装（自色は描かない） | 空実装 | ◯ |
| `draw` | enabledなら子を再帰描画（自身は描かない） | 同等 | ◯ |
